/**************************************************************************
 *
 * OUTAGES.C -  Nagios Network Outages CGI
 *
 * Copyright (c) 1999-2002 Ethan Galstad (nagios@nagios.org)
 * Last Modified: 03-18-2002
 *
 * License:
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *************************************************************************/

#include "../common/config.h"
#include "../common/locations.h"
#include "../common/common.h"
#include "../common/objects.h"
#include "../common/comments.h"
#include "../common/statusdata.h"

#include "cgiutils.h"
#include "getcgi.h"
#include "auth.h"
#include "edata.h"

extern int              refresh_rate;
extern time_t		program_start;

extern hoststatus *hoststatus_list;
extern servicestatus *servicestatus_list;
extern host *host_list;
extern service *service_list;

extern char main_config_file[MAX_FILENAME_LENGTH];
extern char url_html_path[MAX_FILENAME_LENGTH];
extern char url_stylesheets_path[MAX_FILENAME_LENGTH];
extern char url_images_path[MAX_FILENAME_LENGTH];
extern char url_logo_images_path[MAX_FILENAME_LENGTH];
extern char log_file[MAX_FILENAME_LENGTH];


/* HOSTOUTAGE structure */
typedef struct hostoutage_struct{
	host *hst;
	int  severity;
	int  affected_child_hosts;
	int  affected_child_services;
	unsigned long monitored_time;
	unsigned long time_up;
	float percent_time_up;
	unsigned long time_down;
	float percent_time_down;
	unsigned long time_unreachable;
	float percent_time_unreachable;
	struct hostoutage_struct *next;
        }hostoutage;


/* HOSTOUTAGESORT structure */
typedef struct hostoutagesort_struct{
	hostoutage *outage;
	struct hostoutagesort_struct *next;
        }hostoutagesort;


void document_header(int);
void document_footer(void);
int process_cgivars(void);

void display_network_outages(void);
void find_hosts_causing_outages(void);
void calculate_outage_effects(void);
void calculate_outage_effect_of_host(host *,int *,int *);
int is_route_to_host_blocked(host *);
int number_of_host_services(host *);
void add_hostoutage(host *);
void sort_hostoutages(void);
void free_hostoutage_list(void);
void free_hostoutagesort_list(void);


authdata current_authdata;

hostoutage *hostoutage_list=NULL;
hostoutagesort *hostoutagesort_list=NULL;

int service_severity_divisor=4;            /* default = services are 1/4 as important as hosts */

int embedded=FALSE;
int display_header=TRUE;




int main(void){
	int result=OK;
	

	/* get the arguments passed in the URL */
	process_cgivars();

	/* reset internal variables */
	reset_cgi_vars();

	/* read the CGI configuration file */
	result=read_cgi_config_file(DEFAULT_CGI_CONFIG_FILE);
	if(result==ERROR){
		document_header(FALSE);
		cgi_config_file_error(DEFAULT_CGI_CONFIG_FILE);
		document_footer();
		return ERROR;
	        }

	/* read the main configuration file */
	result=read_main_config_file(main_config_file);
	if(result==ERROR){
		document_header(FALSE);
		main_config_file_error(main_config_file);
		document_footer();
		return ERROR;
	        }

	/* read all object configuration data */
	result=read_all_object_configuration_data(main_config_file,READ_HOSTGROUPS|READ_CONTACTGROUPS|READ_HOSTS|READ_SERVICES);
	if(result==ERROR){
		document_header(FALSE);
		object_data_error();
		document_footer();
		return ERROR;
                }

	/* read all status data */
	result=read_all_status_data(DEFAULT_CGI_CONFIG_FILE,READ_PROGRAM_STATUS|READ_HOST_STATUS|READ_SERVICE_STATUS);
	if(result==ERROR){
		document_header(FALSE);
		status_data_error();
		document_footer();
		free_memory();
		return ERROR;
                }

	document_header(TRUE);

	/* read in all host and service comments */
	read_comment_data(DEFAULT_CGI_CONFIG_FILE);

	/* get authentication information */
	get_authentication_information(&current_authdata);

	if(display_header==TRUE){

		/* begin top table */
		printf("<table border=0 width=100%%>\n");
		printf("<tr>\n");

		/* left column of the first row */
		printf("<td align=left valign=top width=33%%>\n");
		display_info_table("Network Outages",TRUE,&current_authdata);
		printf("</td>\n");

		/* middle column of top row */
		printf("<td align=center valign=top width=33%%>\n");
		printf("</td>\n");

		/* right column of top row */
		printf("<td align=right valign=bottom width=33%%>\n");

#ifdef CONTEXT_HELP
		display_context_help(CONTEXTHELP_OUTAGES);
#endif

		printf("</td>\n");

		/* end of top table */
		printf("</tr>\n");
		printf("</table>\n");

	        }


	/* display network outage info */
	display_network_outages();

	document_footer();

	/* free memory allocated to comment data */
	free_comment_data();

	/* free all allocated memory */
	free_memory();

	return OK;
        }



void document_header(int use_stylesheet){
	char date_time[MAX_DATETIME_LENGTH];
	time_t current_time;
	time_t expire_time;

	printf("Cache-Control: no-store\n");
	printf("Pragma: no-cache\n");
	printf("Refresh: %d\n",refresh_rate);

	time(&current_time);
	get_time_string(&current_time,date_time,(int)sizeof(date_time),HTTP_DATE_TIME);
	printf("Last-Modified: %s\n",date_time);

	expire_time=(time_t)0L;
	get_time_string(&expire_time,date_time,(int)sizeof(date_time),HTTP_DATE_TIME);
	printf("Expires: %s\n",date_time);

	printf("Content-type: text/html\r\n\r\n");

	if(embedded==TRUE)
		return;

	printf("<html>\n");
	printf("<head>\n");
	printf("<title>\n");
	printf("Network Outages\n");
	printf("</title>\n");

	if(use_stylesheet==TRUE)
		printf("<LINK REL='stylesheet' TYPE='text/css' HREF='%s%s'>",url_stylesheets_path,OUTAGES_CSS);

	printf("</head>\n");

	printf("<body CLASS='outages'>\n");

	/* include user SSI header */
	include_ssi_files(OUTAGES_CGI,SSI_HEADER);

	return;
        }


void document_footer(void){

	if(embedded==TRUE)
		return;

	/* include user SSI footer */
	include_ssi_files(OUTAGES_CGI,SSI_FOOTER);

	printf("</body>\n");
	printf("</html>\n");

	return;
        }


int process_cgivars(void){
	char **variables;
	int error=FALSE;
	int x;

	variables=getcgivars();

	for(x=0;variables[x]!=NULL;x++){

		/* do some basic length checking on the variable identifier to prevent buffer overflows */
		if(strlen(variables[x])>=MAX_INPUT_BUFFER-1){
			x++;
			continue;
		        }

		/* we found the service severity divisor option */
		if(!strcmp(variables[x],"service_divisor")){
			x++;
			if(variables[x]==NULL){
				error=TRUE;
				break;
			        }

			service_severity_divisor=atoi(variables[x]);
			if(service_severity_divisor<1)
				service_severity_divisor=1;
		        }

		/* we found the embed option */
		else if(!strcmp(variables[x],"embedded"))
			embedded=TRUE;

		/* we found the noheader option */
		else if(!strcmp(variables[x],"noheader"))
			display_header=FALSE;
	        }

	return error;
        }




/* shows all hosts that are causing network outages */
void display_network_outages(void){
	int number_of_problem_hosts=0;
	int number_of_blocking_problem_hosts=0;
	hostoutagesort *temp_hostoutagesort;
	hostoutage *temp_hostoutage;
	hoststatus *temp_hoststatus;
	int odd=0;
	char *bg_class="";
	char *status="";
	unsigned long state_time=0L;
	char time_string[MAX_INPUT_BUFFER];
	int days;
	int hours;
	int minutes;
	int seconds;
	int total_comments;
	time_t t;
	time_t current_time;
	char state_duration[48];
	int total_entries=0;

	/* user must be authorized for all hosts.. */
	if(is_authorized_for_all_hosts(&current_authdata)==FALSE){

		printf("<P><DIV CLASS='errorMessage'>It appears as though you do not have permission to view information you requested...</DIV></P>\n");
		printf("<P><DIV CLASS='errorDescription'>If you believe this is an error, check the HTTP server authentication requirements for accessing this CGI<br>");
		printf("and check the authorization options in your CGI configuration file.</DIV></P>\n");

		return;
	        }

	/* find all hosts that are causing network outages */
	find_hosts_causing_outages();

	/* calculate outage effects */
	calculate_outage_effects();

	/* sort the outage list by severity */
	sort_hostoutages();

	/* count the number of top-level hosts that are down and the ones that are actually blocking children hosts */
	for(temp_hostoutage=hostoutage_list;temp_hostoutage!=NULL;temp_hostoutage=temp_hostoutage->next){
		number_of_problem_hosts++;
		if(temp_hostoutage->affected_child_hosts>1)
			number_of_blocking_problem_hosts++;
	        }

	/* display the problem hosts... */
	printf("<P><DIV ALIGN=CENTER>\n");
	printf("<DIV CLASS='dataTitle'>Blocking Outages</DIV>\n");

	printf("<TABLE BORDER=0 CLASS='data'>\n");
	printf("<TR>\n");
	printf("<TH CLASS='data'>Severity</TH><TH CLASS='data'>Host</TH><TH CLASS='data'>State</TH><TH CLASS='data'>Notes</TH><TH CLASS='data'>State Duration</TH><TH CLASS='data'>Total State Time</TH><TH CLASS='data'># Hosts Affected</TH><TH CLASS='data'># Services Affected</TH><TH CLASS='data'>Actions</TH>\n");
	printf("</TR>\n");

	for(temp_hostoutagesort=hostoutagesort_list;temp_hostoutagesort!=NULL;temp_hostoutagesort=temp_hostoutagesort->next){

		temp_hostoutage=temp_hostoutagesort->outage;
		if(temp_hostoutage==NULL)
			continue;

		/* skip hosts that are not blocking anyone */
		if(temp_hostoutage->affected_child_hosts<=1)
			continue;

		temp_hoststatus=find_hoststatus(temp_hostoutage->hst->name);
		if(temp_hoststatus==NULL)
			continue;

		/* make	sure we only caught valid state types */
		if(temp_hoststatus->status!=HOST_DOWN && temp_hoststatus->status!=HOST_UNREACHABLE)
			continue;

		total_entries++;

		if(odd==0){
			odd=1;
			bg_class="dataOdd";
		        }
		else{
			odd=0;
			bg_class="dataEven";
		        }

		if(temp_hoststatus->status==HOST_UNREACHABLE){
			state_time=temp_hostoutage->time_unreachable;
			status="UNREACHABLE";
		        }
		else if(temp_hoststatus->status==HOST_DOWN){
			state_time=temp_hostoutage->time_down;
			status="DOWN";
		        }

		get_time_breakdown(state_time,&days,&hours,&minutes,&seconds);
		snprintf(time_string,sizeof(time_string)-1,"%dd %dh %dm %ds",days,hours,minutes,seconds);

		printf("<TR CLASS='%s'>\n",bg_class);

		printf("<TD CLASS='%s'>%d</TD>\n",bg_class,temp_hostoutage->severity);
		printf("<TD CLASS='%s'><A HREF='%s?type=%d&host=%s'>%s</A></TD>\n",bg_class,EXTINFO_CGI,DISPLAY_HOST_INFO,url_encode(temp_hostoutage->hst->name),temp_hostoutage->hst->name);
		printf("<TD CLASS='host%s'>%s</TD>\n",status,status);

		total_comments=number_of_host_comments(temp_hostoutage->hst->name);
		if(total_comments>0)
			printf("<TD CLASS='%s'><A HREF='%s?type=%d&host=%s#comments'><IMG SRC='%s%s' BORDER=0 ALT='This host has %d comment%s associated with it'></A></TD>\n",bg_class,EXTINFO_CGI,DISPLAY_HOST_INFO,url_encode(temp_hostoutage->hst->name),url_images_path,COMMENT_ICON,total_comments,(total_comments==1)?"":"s");
		else
			printf("<TD CLASS='%s'>N/A</TD>\n",bg_class);



		current_time=time(NULL);
		if(temp_hoststatus->last_state_change==(time_t)0)
			t=current_time-program_start;
		else
			t=current_time-temp_hoststatus->last_state_change;
		get_time_breakdown((unsigned long)t,&days,&hours,&minutes,&seconds);
		snprintf(state_duration,sizeof(state_duration)-1,"%2dd %2dh %2dm %2ds%s",days,hours,minutes,seconds,(temp_hoststatus->last_state_change==(time_t)0)?"+":"");
		state_duration[sizeof(state_duration)-1]='\x0';
		printf("<TD CLASS='%s'>%s</TD>\n",bg_class,state_duration);

		printf("<TD CLASS='%s'>%s</TD>\n",bg_class,time_string);
		printf("<TD CLASS='%s'>%d</TD>\n",bg_class,temp_hostoutage->affected_child_hosts);
		printf("<TD CLASS='%s'>%d</TD>\n",bg_class,temp_hostoutage->affected_child_services);

		printf("<TD CLASS='%s'>",bg_class);
		printf("<A HREF='%s?host=%s'><IMG SRC='%s%s' BORDER=0 ALT='View status detail for this host'></A>\n",STATUS_CGI,url_encode(temp_hostoutage->hst->name),url_images_path,STATUS_DETAIL_ICON);
#ifdef USE_STATUSMAP
		printf("<A HREF='%s?host=%s'><IMG SRC='%s%s' BORDER=0 ALT='View status map for this host and its children'></A>\n",STATUSMAP_CGI,url_encode(temp_hostoutage->hst->name),url_images_path,STATUSMAP_ICON);
#endif
#ifdef USE_STATUSWRL
		printf("<A HREF='%s?host=%s'><IMG SRC='%s%s' BORDER=0 ALT='View 3-D status map for this host and its children'></A>\n",STATUSWORLD_CGI,url_encode(temp_hostoutage->hst->name),url_images_path,STATUSWORLD_ICON);
#endif
#ifdef USE_TRENDS
		printf("<A HREF='%s?host=%s'><IMG SRC='%s%s' BORDER=0 ALT='View trends for this host'></A>\n",TRENDS_CGI,url_encode(temp_hostoutage->hst->name),url_images_path,TRENDS_ICON);
#endif
		printf("<A HREF='%s?host=%s'><IMG SRC='%s%s' BORDER=0 ALT='View alert history for this host'></A>\n",HISTORY_CGI,url_encode(temp_hostoutage->hst->name),url_images_path,HISTORY_ICON);
		printf("<A HREF='%s?host=%s'><IMG SRC='%s%s' BORDER=0 ALT='View notifications for this host'></A>\n",NOTIFICATIONS_CGI,url_encode(temp_hostoutage->hst->name),url_images_path,NOTIFICATION_ICON);
		printf("</TD>\n");

		printf("</TR>\n");
	        }

	printf("</TABLE>\n");

	printf("</DIV></P>\n");

	if(total_entries==0)
		printf("<DIV CLASS='itemTotalsTitle'>%d Blocking Outages Displayed</DIV>\n",total_entries);

	/* free memory allocated to the host outage list */
	free_hostoutage_list();
	free_hostoutagesort_list();

	return;
        }





/* determine what hosts are causing network outages */
void find_hosts_causing_outages(void){
	hoststatus *temp_hoststatus;
	host *temp_host;

	/* check all hosts */
	for(temp_hoststatus=hoststatus_list;temp_hoststatus!=NULL;temp_hoststatus=temp_hoststatus->next){

		/* check only hosts that are not up and not pending */
		if(temp_hoststatus->status!=HOST_UP && temp_hoststatus->status!=HOST_PENDING){

			/* find the host entry */
			temp_host=find_host(temp_hoststatus->host_name,NULL);

			if(temp_host==NULL)
				continue;

			/* if the route to this host is not blocked, it is a causing an outage */
			if(is_route_to_host_blocked(temp_host)==FALSE)
				add_hostoutage(temp_host);
		        }
	        }

	return;
        }





/* adds a host outage entry */
void add_hostoutage(host *hst){
	hostoutage *new_hostoutage;

	/* allocate memory for a new structure */
	new_hostoutage=(hostoutage *)malloc(sizeof(hostoutage));

	if(new_hostoutage==NULL)
		return;

	new_hostoutage->hst=hst;
	new_hostoutage->severity=0;
	new_hostoutage->affected_child_hosts=0;
	new_hostoutage->affected_child_services=0;

	/* add the structure to the head of the list in memory */
	new_hostoutage->next=hostoutage_list;
	hostoutage_list=new_hostoutage;

	return;
        }



/* frees all memory allocated to the host outage list */
void free_hostoutage_list(void){
	hostoutage *this_hostoutage;
	hostoutage *next_hostoutage;

	/* free all list members */
	for(this_hostoutage=hostoutage_list;this_hostoutage!=NULL;this_hostoutage=next_hostoutage){
		next_hostoutage=this_hostoutage->next;
		free(this_hostoutage);
	        }

	/* reset list pointer */
	hostoutage_list=NULL;

	return;
        }



/* frees all memory allocated to the host outage sort list */
void free_hostoutagesort_list(void){
	hostoutagesort *this_hostoutagesort;
	hostoutagesort *next_hostoutagesort;

	/* free all list members */
	for(this_hostoutagesort=hostoutagesort_list;this_hostoutagesort!=NULL;this_hostoutagesort=next_hostoutagesort){
		next_hostoutagesort=this_hostoutagesort->next;
		free(this_hostoutagesort);
	        }

	/* reset list pointer */
	hostoutagesort_list=NULL;

	return;
        }



/* calculates network outage effect of all hosts that are causing blockages */
void calculate_outage_effects(void){
	hostoutage *temp_hostoutage;

	/* check all hosts causing problems */
	for(temp_hostoutage=hostoutage_list;temp_hostoutage!=NULL;temp_hostoutage=temp_hostoutage->next){

		/* calculate the outage effect of this particular hosts */
		calculate_outage_effect_of_host(temp_hostoutage->hst,&temp_hostoutage->affected_child_hosts,&temp_hostoutage->affected_child_services);

		temp_hostoutage->severity=(temp_hostoutage->affected_child_hosts+(temp_hostoutage->affected_child_services/service_severity_divisor));

		calculate_host_state_times(temp_hostoutage->hst->name,&temp_hostoutage->monitored_time,&temp_hostoutage->time_up,&temp_hostoutage->percent_time_up,&temp_hostoutage->time_down,&temp_hostoutage->percent_time_down,&temp_hostoutage->time_unreachable,&temp_hostoutage->percent_time_unreachable);
	        }

	return;
        }




/* calculates network outage effect of a particular host being down or unreachable */
void calculate_outage_effect_of_host(host *hst, int *affected_hosts, int *affected_services){
	int total_child_hosts_affected=0;
	int total_child_services_affected=0;
	int temp_child_hosts_affected=0;
	int temp_child_services_affected=0;
	host *temp_host;


	/* find all child hosts of this host */
	for(temp_host=host_list;temp_host!=NULL;temp_host=temp_host->next){

		/* skip this host if it is not a child */
		if(is_host_immediate_child_of_host(hst,temp_host)==FALSE)
			continue;

		/* calculate the outage effect of the child */
		calculate_outage_effect_of_host(temp_host,&temp_child_hosts_affected,&temp_child_services_affected);

		/* keep a running total of outage effects */
		total_child_hosts_affected+=temp_child_hosts_affected;
		total_child_services_affected+=temp_child_services_affected;
	        }

	*affected_hosts=total_child_hosts_affected+1;
	*affected_services=total_child_services_affected+number_of_host_services(hst);

	return;
        }



/* tests whether or not a host is "blocked" by upstream parents (host is already assumed to be down or unreachable) */
int is_route_to_host_blocked(host *hst){
	hostsmember *temp_hostsmember;
	hoststatus *temp_hoststatus;

	/* if the host has no parents, it is not being blocked by anyone */
	if(hst->parent_hosts==NULL)
		return FALSE;

	/* check all parent hosts */
	for(temp_hostsmember=hst->parent_hosts;temp_hostsmember!=NULL;temp_hostsmember=temp_hostsmember->next){

		/* find the parent host's status */
		temp_hoststatus=find_hoststatus(temp_hostsmember->host_name);

		if(temp_hoststatus==NULL)
			continue;

		/* at least one parent it up (or pending), so this host is not blocked */
		if(temp_hoststatus->status==HOST_UP || temp_hoststatus->status==HOST_PENDING)
			return FALSE;
	        }

	return TRUE;
        }



/* calculates the number of services associated a particular host */
int number_of_host_services(host *hst){
	int total_services=0;
	service *temp_service;

	/* check all services */
	for(temp_service=service_list;temp_service!=NULL;temp_service=temp_service->next){

		/* we found a service on this host */
		if(!strcmp(hst->name,temp_service->host_name))
			total_services++;
	        }

	return total_services;
        }



/* sort the host outages by severity */
void sort_hostoutages(void){
	hostoutagesort *last_hostoutagesort;
	hostoutagesort *new_hostoutagesort;
	hostoutagesort *temp_hostoutagesort;
	hostoutage *temp_hostoutage;

	if(hostoutage_list==NULL)
		return;

	/* sort all host outage entries */
	for(temp_hostoutage=hostoutage_list;temp_hostoutage!=NULL;temp_hostoutage=temp_hostoutage->next){

		/* allocate memory for a new sort structure */
		new_hostoutagesort=(hostoutagesort *)malloc(sizeof(hostoutagesort));
		if(new_hostoutagesort==NULL)
			return;

		new_hostoutagesort->outage=temp_hostoutage;

		last_hostoutagesort=hostoutagesort_list;
		for(temp_hostoutagesort=hostoutagesort_list;temp_hostoutagesort!=NULL;temp_hostoutagesort=temp_hostoutagesort->next){

			if(new_hostoutagesort->outage->severity >= temp_hostoutagesort->outage->severity){
				new_hostoutagesort->next=temp_hostoutagesort;
				if(temp_hostoutagesort==hostoutagesort_list)
					hostoutagesort_list=new_hostoutagesort;
				else
					last_hostoutagesort->next=new_hostoutagesort;
				break;
		                }
			else
				last_hostoutagesort=temp_hostoutagesort;
	                }

		if(hostoutagesort_list==NULL){
			new_hostoutagesort->next=NULL;
			hostoutagesort_list=new_hostoutagesort;
	                }
		else if(temp_hostoutagesort==NULL){
			new_hostoutagesort->next=NULL;
			last_hostoutagesort->next=new_hostoutagesort;
	                }
	        }

	return;
        }