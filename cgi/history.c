/***********************************************************************
 *
 * HISTORY.C - Nagios History CGI
 *
 * Copyright (c) 1999-2002 Ethan Galstad (nagios@nagios.org)
 * Last Modified: 03-08-2002
 *
 * This CGI program will display the history for the specified host.
 * If no host is specified, the history for all hosts will be displayed.
 *
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
 ***********************************************************************/

#include "../common/config.h"
#include "../common/locations.h"
#include "../common/common.h"
#include "../common/objects.h"

#include "getcgi.h"
#include "cgiutils.h"
#include "auth.h"
#include "lifo.h"

#define DISPLAY_HOSTS			0
#define DISPLAY_SERVICES		1

#define SERVICE_HISTORY			0
#define HOST_HISTORY			1
#define SERVICE_FLAPPING_HISTORY        2
#define HOST_FLAPPING_HISTORY           3
#define SERVICE_DOWNTIME_HISTORY        4
#define HOST_DOWNTIME_HISTORY           5

#define STATE_ALL			0
#define STATE_SOFT			1
#define STATE_HARD			2

void get_history(void);

void document_header(int);
void document_footer(void);
int process_cgivars(void);

extern char main_config_file[MAX_FILENAME_LENGTH];
extern char url_images_path[MAX_FILENAME_LENGTH];
extern char url_stylesheets_path[MAX_FILENAME_LENGTH];

extern int log_rotation_method;

authdata current_authdata;

char log_file_to_use[MAX_FILENAME_LENGTH];
int log_archive=0;

int show_all_hosts=TRUE;
char *host_name="all";
char *svc_description="";
int display_type=DISPLAY_HOSTS;
int use_lifo=TRUE;

int history_options=HISTORY_ALL;
int state_options=STATE_ALL;

extern host *host_list;

int embedded=FALSE;
int display_header=TRUE;
int display_frills=TRUE;
int display_timebreaks=TRUE;
int display_system_messages=TRUE;
int display_flapping_alerts=TRUE;
int display_downtime_alerts=TRUE;


int main(void){
	int result=OK;
	char temp_buffer[MAX_INPUT_BUFFER];
	char temp_buffer2[MAX_INPUT_BUFFER];

	/* get the variables passed to us */
	process_cgivars();

	/* reset internal CGI variables */
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
	result=read_all_object_configuration_data(main_config_file,READ_HOSTGROUPS|READ_CONTACTGROUPS|READ_CONTACTS|READ_HOSTS|READ_SERVICES);
	if(result==ERROR){
		document_header(FALSE);
		object_data_error();
		document_footer();
		return ERROR;
                }

	document_header(TRUE);

	/* get authentication information */
	get_authentication_information(&current_authdata);

	/* determine what log file we should be using */
	get_log_archive_to_use(log_archive,log_file_to_use,(int)sizeof(log_file_to_use));

	if(display_header==TRUE){

		/* begin top table */
		printf("<table border=0 width=100%%>\n");
		printf("<tr>\n");

		/* left column of the first row */
		printf("<td align=left valign=top width=33%%>\n");

		if(display_type==DISPLAY_SERVICES)
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"Service Alert History");
		else if(show_all_hosts==TRUE)
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"Alert History");
		else
			snprintf(temp_buffer,sizeof(temp_buffer)-1,"Host Alert History");
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		display_info_table(temp_buffer,FALSE,&current_authdata);

		printf("<TABLE BORDER=1 CELLPADDING=0 CELLSPACING=0 CLASS='linkBox'>\n");
		printf("<TR><TD CLASS='linkBox'>\n");
		if(display_type==DISPLAY_HOSTS){
			printf("<A HREF='%s?host=%s'>View Status Detail For %s</A><BR>\n",STATUS_CGI,(show_all_hosts==TRUE)?"all":url_encode(host_name),(show_all_hosts==TRUE)?"All Hosts":"This Host");
			printf("<A HREF='%s?host=%s'>View Notifications For %s</A><BR>\n",NOTIFICATIONS_CGI,(show_all_hosts==TRUE)?"all":url_encode(host_name),(show_all_hosts==TRUE)?"All Hosts":"This Host");
#ifdef USE_TRENDS
			if(show_all_hosts==FALSE)
				printf("<A HREF='%s?host=%s'>View Trends For This Host</A>\n",TRENDS_CGI,url_encode(host_name));
#endif
	                }
		else{
			printf("<A HREF='%s?host=%s&",NOTIFICATIONS_CGI,url_encode(host_name));
			printf("service=%s'>View Notifications For This Service</A><BR>\n",url_encode(svc_description));
#ifdef USE_TRENDS
			printf("<A HREF='%s?host=%s&",TRENDS_CGI,url_encode(host_name));
			printf("service=%s'>View Trends For This Service</A><BR>\n",url_encode(svc_description));
#endif
			printf("<A HREF='%s?host=%s'>View History For This Host</A>\n",HISTORY_CGI,url_encode(host_name));
	                }
		printf("</TD></TR>\n");
		printf("</TABLE>\n");

		printf("</td>\n");


		/* middle column of top row */
		printf("<td align=center valign=top width=33%%>\n");

		printf("<DIV ALIGN=CENTER CLASS='dataTitle'>\n");
		if(display_type==DISPLAY_SERVICES)
			printf("Service '%s' On Host '%s'",svc_description,host_name);
		else if(show_all_hosts==TRUE)
			printf("All Hosts and Services");
		else
			printf("Host '%s'",host_name);
		printf("</DIV>\n");
		printf("<BR>\n");

		snprintf(temp_buffer,sizeof(temp_buffer)-1,"%s?%shost=%s&type=%d&statetype=%d&",HISTORY_CGI,(use_lifo==FALSE)?"oldestfirst&":"",url_encode(host_name),history_options,state_options);
		temp_buffer[sizeof(temp_buffer)-1]='\x0';
		if(display_type==DISPLAY_SERVICES){
			snprintf(temp_buffer2,sizeof(temp_buffer2)-1,"service=%s&",url_encode(svc_description));
			temp_buffer2[sizeof(temp_buffer2)-1]='\x0';
			strncat(temp_buffer,temp_buffer2,sizeof(temp_buffer)-strlen(temp_buffer)-1);
			temp_buffer[sizeof(temp_buffer)-1]='\x0';
	                }
		display_nav_table(temp_buffer,log_archive);

		printf("</td>\n");


		/* right hand column of top row */
		printf("<td align=right valign=top width=33%%>\n");

		printf("<table border=0 CLASS='optBox'>\n");
		printf("<form method=\"GET\" action=\"%s\">\n",HISTORY_CGI);
		printf("<input type='hidden' name='host' value='%s'>\n",(show_all_hosts==TRUE)?"all":host_name);
		if(display_type==DISPLAY_SERVICES)
			printf("<input type='hidden' name='service' value='%s'>\n",svc_description);
		printf("<input type='hidden' name='archive' value='%d'>\n",log_archive);

		printf("<tr>\n");
		printf("<td align=left CLASS='optBoxItem'>State type options:</td>\n");
		printf("</tr>\n");

		printf("<tr>\n");
		printf("<td align=left CLASS='optBoxItem'><select name='statetype'>\n");
		printf("<option value=%d %s>All state types\n",STATE_ALL,(state_options==STATE_ALL)?"selected":"");
		printf("<option value=%d %s>Soft states\n",STATE_SOFT,(state_options==STATE_SOFT)?"selected":"");
		printf("<option value=%d %s>Hard states\n",STATE_HARD,(state_options==STATE_HARD)?"selected":"");
		printf("</select></td>\n");
		printf("</tr>\n");

		printf("<tr>\n");
		printf("<td align=left CLASS='optBoxItem'>History detail level for ");
		if(display_type==DISPLAY_HOSTS)
			printf("%s host%s",(show_all_hosts==TRUE)?"all":"this",(show_all_hosts==TRUE)?"s":"");
		else
			printf("service");
		printf(":</td>\n");
		printf("</tr>\n")
;
		printf("<tr>\n");
		printf("<td align=left CLASS='optBoxItem'><select name='type'>\n");
		if(display_type==DISPLAY_HOSTS)
			printf("<option value=%d %s>All alerts\n",HISTORY_ALL,(history_options==HISTORY_ALL)?"selected":"");
		printf("<option value=%d %s>All service alerts\n",HISTORY_SERVICE_ALL,(history_options==HISTORY_SERVICE_ALL)?"selected":"");
		if(display_type==DISPLAY_HOSTS)
			printf("<option value=%d %s>All host alerts\n",HISTORY_HOST_ALL,(history_options==HISTORY_HOST_ALL)?"selected":"");
		printf("<option value=%d %s>Service warning\n",HISTORY_SERVICE_WARNING,(history_options==HISTORY_SERVICE_WARNING)?"selected":"");
		printf("<option value=%d %s>Service unknown\n",HISTORY_SERVICE_UNKNOWN,(history_options==HISTORY_SERVICE_UNKNOWN)?"selected":"");
		printf("<option value=%d %s>Service critical\n",HISTORY_SERVICE_CRITICAL,(history_options==HISTORY_SERVICE_CRITICAL)?"selected":"");
		printf("<option value=%d %s>Service recovery\n",HISTORY_SERVICE_RECOVERY,(history_options==HISTORY_SERVICE_RECOVERY)?"selected":"");
		if(display_type==DISPLAY_HOSTS){
			printf("<option value=%d %s>Host down\n",HISTORY_HOST_DOWN,(history_options==HISTORY_HOST_DOWN)?"selected":"");
			printf("<option value=%d %s>Host unreachable\n",HISTORY_HOST_UNREACHABLE,(history_options==HISTORY_HOST_UNREACHABLE)?"selected":"");
		        printf("<option value=%d %s>Host recovery\n",HISTORY_HOST_RECOVERY,(history_options==HISTORY_HOST_RECOVERY)?"selected":"");
	                }
		printf("</select></td>\n");
		printf("</tr>\n");

		printf("<tr>\n");
		printf("<td align=left valign=bottom CLASS='optBoxItem'><input type='checkbox' name='noflapping' %s> Hide Flapping Alerts</td>",(display_flapping_alerts==FALSE)?"checked":"");
		printf("</tr>\n");
		printf("<tr>\n");
		printf("<td align=left valign=bottom CLASS='optBoxItem'><input type='checkbox' name='nodowntime' %s> Hide Downtime Alerts</td>",(display_downtime_alerts==FALSE)?"checked":"");
		printf("</tr>\n");

		printf("<tr>\n");
		printf("<td align=left valign=bottom CLASS='optBoxItem'><input type='checkbox' name='nosystem' %s> Hide Process Messages</td>",(display_system_messages==FALSE)?"checked":"");
		printf("</tr>\n");
		printf("<tr>\n");
		printf("<td align=left valign=bottom CLASS='optBoxItem'><input type='checkbox' name='oldestfirst' %s> Older Entries First</td>",(use_lifo==FALSE)?"checked":"");
		printf("</tr>\n");

		printf("<tr>\n");
		printf("<td align=left CLASS='optBoxItem'><input type='submit' value='Update'></td>\n");
		printf("</tr>\n");

#ifdef CONTEXT_HELP
		printf("<tr>\n");
		printf("<td align=right>\n");
		display_context_help(CONTEXTHELP_HISTORY);
		printf("</td>\n");
		printf("</tr>\n");
#endif

		printf("</form>\n");
		printf("</table>\n");

		printf("</td>\n");

		/* end of top table */
		printf("</tr>\n");
		printf("</table>\n");

	        }


	/* display history */
	get_history();

	/* free allocated memory */
	free_memory();
	
	return OK;
        }



void document_header(int use_stylesheet){
	char date_time[MAX_DATETIME_LENGTH];
	time_t current_time;
	time_t expire_time;

	printf("Cache-Control: no-store\n");
	printf("Pragma: no-cache\n");

	time(&current_time);
	get_time_string(&current_time,date_time,sizeof(date_time),HTTP_DATE_TIME);
	printf("Last-Modified: %s\n",date_time);

	expire_time=(time_t)0L;
	get_time_string(&expire_time,date_time,sizeof(date_time),HTTP_DATE_TIME);
	printf("Expires: %s\n",date_time);

	printf("Content-type: text/html\n\n");

	if(embedded==TRUE)
		return;

	printf("<html>\n");
	printf("<head>\n");
	printf("<title>\n");
	printf("Nagios History\n");
	printf("</title>\n");

	if(use_stylesheet==TRUE)
		printf("<LINK REL='stylesheet' TYPE='text/css' HREF='%s%s'>\n",url_stylesheets_path,HISTORY_CSS);
	
	printf("</head>\n");
	printf("<BODY CLASS='history'>\n");

	/* include user SSI header */
	include_ssi_files(HISTORY_CGI,SSI_HEADER);

	return;
        }


void document_footer(void){

	if(embedded==TRUE)
		return;

	/* include user SSI footer */
	include_ssi_files(HISTORY_CGI,SSI_FOOTER);

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
		if(strlen(variables[x])>=MAX_INPUT_BUFFER-1)
			continue;

		/* we found the host argument */
		else if(!strcmp(variables[x],"host")){
			x++;
			if(variables[x]==NULL){
				error=TRUE;
				break;
			        }

			host_name=(char *)malloc(strlen(variables[x])+1);
			if(host_name==NULL)
				host_name="";
			else
				strcpy(host_name,variables[x]);

			display_type=DISPLAY_HOSTS;

			if(!strcmp(host_name,"all"))
				show_all_hosts=TRUE;
			else
				show_all_hosts=FALSE;
		        }
	
		/* we found the service argument */
		else if(!strcmp(variables[x],"service")){
			x++;
			if(variables[x]==NULL){
				error=TRUE;
				break;
			        }

			svc_description=(char *)malloc(strlen(variables[x])+1);
			if(svc_description==NULL)
				svc_description="";
			else
				strcpy(svc_description,variables[x]);

			display_type=DISPLAY_SERVICES;
		        }
	
	
		/* we found the history type argument */
		else if(!strcmp(variables[x],"type")){
			x++;
			if(variables[x]==NULL){
				error=TRUE;
				break;
			        }

			history_options=atoi(variables[x]);
		        }
	
		/* we found the history state type argument */
		else if(!strcmp(variables[x],"statetype")){
			x++;
			if(variables[x]==NULL){
				error=TRUE;
				break;
			        }

			state_options=atoi(variables[x]);
		        }
	
	
		/* we found the log archive argument */
		else if(!strcmp(variables[x],"archive")){
			x++;
			if(variables[x]==NULL){
				error=TRUE;
				break;
			        }

			log_archive=atoi(variables[x]);
			if(log_archive<0)
				log_archive=0;
		        }

		/* we found the order argument */
		else if(!strcmp(variables[x],"oldestfirst")){
			use_lifo=FALSE;
		        }

		/* we found the embed option */
		else if(!strcmp(variables[x],"embedded"))
			embedded=TRUE;

		/* we found the noheader option */
		else if(!strcmp(variables[x],"noheader"))
			display_header=FALSE;

		/* we found the nofrills option */
		else if(!strcmp(variables[x],"nofrills"))
			display_frills=FALSE;

		/* we found the notimebreaks option */
		else if(!strcmp(variables[x],"notimebreaks"))
			display_timebreaks=FALSE;

		/* we found the no system messages option */
		else if(!strcmp(variables[x],"nosystem"))
			display_system_messages=FALSE;

		/* we found the no flapping alerts option */
		else if(!strcmp(variables[x],"noflapping"))
			display_flapping_alerts=FALSE;

		/* we found the no downtime alerts option */
		else if(!strcmp(variables[x],"nodowntime"))
			display_downtime_alerts=FALSE;
	        }

	return error;
        }



void get_history(void){
	FILE *fp=NULL;
	char image[MAX_INPUT_BUFFER];
	char image_alt[MAX_INPUT_BUFFER];
	char input_buffer[MAX_INPUT_BUFFER];
	char input_buffer2[MAX_INPUT_BUFFER];
	char match1[MAX_INPUT_BUFFER];
	char match2[MAX_INPUT_BUFFER];
	int found_line=FALSE;
	int system_message=FALSE;
	int display_line=FALSE;
	time_t t;
	char date_time[MAX_DATETIME_LENGTH];
	char *temp_buffer;
	int history_type=SERVICE_HISTORY;
	int history_detail_type=HISTORY_SERVICE_CRITICAL;
	char entry_host_name[MAX_HOSTNAME_LENGTH];
	char entry_service_desc[MAX_SERVICEDESC_LENGTH];
	host *temp_host;
	service *temp_service;
	int result;

	char last_message_date[MAX_INPUT_BUFFER]="";
	char current_message_date[MAX_INPUT_BUFFER]="";
	struct tm *time_ptr;


	if(use_lifo==TRUE){
		result=read_file_into_lifo(log_file_to_use);
		if(result!=LIFO_OK){
			if(result==LIFO_ERROR_MEMORY){
				printf("<P><DIV CLASS='warningMessage'>Not enough memory to reverse log file - displaying history in natural order...</DIV></P>\n");
			        }
			else if(result==LIFO_ERROR_FILE){
				printf("<HR><P><DIV CLASS='errorMessage'>Error: Cannot open log file '%s' for reading!</DIV></P><HR>",log_file_to_use);
				return;
			        }
			use_lifo=FALSE;
		        }
	        }

	if(use_lifo==FALSE){

		fp=fopen(log_file_to_use,"r");
		if(fp==NULL){
			printf("<HR><P><DIV CLASS='errorMessage'>Error: Cannot open log file '%s' for reading!</DIV></P><HR>",log_file_to_use);
			return;
		        }
	        }

	printf("<P><DIV CLASS='logEntries'\n");

	while(1){

		if(use_lifo==TRUE){
			if(pop_lifo(input_buffer,(int)sizeof(input_buffer))!=LIFO_OK)
				break;
		        }
		else{
			fgets(input_buffer,(int)(sizeof(input_buffer)-1),fp);
			if(feof(fp))
				break;
		        }

		strcpy(image,"");
		strcpy(image_alt,"");
		system_message=FALSE;

		strcpy(input_buffer2,input_buffer);

		/* service state alerts */
		if(strstr(input_buffer,"SERVICE ALERT:")){
			
			history_type=SERVICE_HISTORY;

			/* get host and service names */
			temp_buffer=my_strtok(input_buffer2,"]");
			temp_buffer=my_strtok(NULL,":");
			temp_buffer=my_strtok(NULL,";");
			strncpy(entry_host_name,(temp_buffer==NULL)?"":temp_buffer+1,sizeof(entry_host_name));
			entry_host_name[sizeof(entry_host_name)-1]='\x0';
			temp_buffer=my_strtok(NULL,";");
			strncpy(entry_service_desc,(temp_buffer==NULL)?"":temp_buffer,sizeof(entry_service_desc));
			entry_service_desc[sizeof(entry_service_desc)-1]='\x0';

			if(strstr(input_buffer,";CRITICAL;")){
				strncpy(image,CRITICAL_ICON,sizeof(image));
				strncpy(image_alt,CRITICAL_ICON_ALT,sizeof(image_alt));
				history_detail_type=HISTORY_SERVICE_CRITICAL;
                                }
			else if(strstr(input_buffer,";WARNING;")){
				strncpy(image,WARNING_ICON,sizeof(image));
				strncpy(image_alt,WARNING_ICON_ALT,sizeof(image_alt));
				history_detail_type=HISTORY_SERVICE_WARNING;
                                }
			else if(strstr(input_buffer,";UNKNOWN;")){
				strncpy(image,UNKNOWN_ICON,sizeof(image));
				strncpy(image_alt,UNKNOWN_ICON_ALT,sizeof(image_alt));
 				history_detail_type=HISTORY_SERVICE_UNKNOWN;
                                }
			else if(strstr(input_buffer,";RECOVERY;") || strstr(input_buffer,";OK;")){
				strncpy(image,OK_ICON,sizeof(image));
				strncpy(image_alt,OK_ICON_ALT,sizeof(image_alt));
				history_detail_type=HISTORY_SERVICE_RECOVERY;
                                }
		        }

		/* service flapping alerts */
		else if(strstr(input_buffer,"SERVICE FLAPPING ALERT:")){

			if(display_flapping_alerts==FALSE)
				continue;
			
			history_type=SERVICE_FLAPPING_HISTORY;

			/* get host and service names */
			temp_buffer=my_strtok(input_buffer2,"]");
			temp_buffer=my_strtok(NULL,":");
			temp_buffer=my_strtok(NULL,";");
			strncpy(entry_host_name,(temp_buffer==NULL)?"":temp_buffer+1,sizeof(entry_host_name));
			entry_host_name[sizeof(entry_host_name)-1]='\x0';
			temp_buffer=my_strtok(NULL,";");
			strncpy(entry_service_desc,(temp_buffer==NULL)?"":temp_buffer,sizeof(entry_service_desc));
			entry_service_desc[sizeof(entry_service_desc)-1]='\x0';

			strncpy(image,FLAPPING_ICON,sizeof(image));

			if(strstr(input_buffer,";STARTED;"))
			        strncpy(image_alt,"Service started flapping",sizeof(image_alt));
			else if(strstr(input_buffer,";STOPPED;"))
			        strncpy(image_alt,"Service stopped flapping",sizeof(image_alt));
			else if(strstr(input_buffer,";DISABLED;"))
			        strncpy(image_alt,"Service flap detection disabled",sizeof(image_alt));
		        }

		/* service downtime alerts */
		else if(strstr(input_buffer,"SERVICE DOWNTIME ALERT:")){
			
			if(display_downtime_alerts==FALSE)
				continue;
			
			history_type=SERVICE_DOWNTIME_HISTORY;

			/* get host and service names */
			temp_buffer=my_strtok(input_buffer2,"]");
			temp_buffer=my_strtok(NULL,":");
			temp_buffer=my_strtok(NULL,";");
			strncpy(entry_host_name,(temp_buffer==NULL)?"":temp_buffer+1,sizeof(entry_host_name));
			entry_host_name[sizeof(entry_host_name)-1]='\x0';
			temp_buffer=my_strtok(NULL,";");
			strncpy(entry_service_desc,(temp_buffer==NULL)?"":temp_buffer,sizeof(entry_service_desc));
			entry_service_desc[sizeof(entry_service_desc)-1]='\x0';

			strncpy(image,SCHEDULED_DOWNTIME_ICON,sizeof(image));

			if(strstr(input_buffer,";STARTED;"))
			        strncpy(image_alt,"Service entered a period of scheduled downtime",sizeof(image_alt));
			else if(strstr(input_buffer,";STOPPED;"))
			        strncpy(image_alt,"Service exited from a period of scheduled downtime",sizeof(image_alt));
			else if(strstr(input_buffer,";CANCELLED;"))
			        strncpy(image_alt,"Service scheduled downtime has been cancelled",sizeof(image_alt));
		        }

		/* host state alerts */
		else if(strstr(input_buffer,"HOST ALERT:")){

			history_type=HOST_HISTORY;

			/* get host name */
			temp_buffer=my_strtok(input_buffer2,"]");
			temp_buffer=my_strtok(NULL,":");
			temp_buffer=my_strtok(NULL,";");
			strncpy(entry_host_name,(temp_buffer==NULL)?"":temp_buffer+1,sizeof(entry_host_name));
			entry_host_name[sizeof(entry_host_name)-1]='\x0';

			if(strstr(input_buffer,";DOWN;")){
				strncpy(image,HOST_DOWN_ICON,sizeof(image));
				strncpy(image_alt,HOST_DOWN_ICON_ALT,sizeof(image_alt));
				history_detail_type=HISTORY_HOST_DOWN;
		                }
			else if(strstr(input_buffer,";UNREACHABLE;")){
				strncpy(image,HOST_UNREACHABLE_ICON,sizeof(image));
				strncpy(image_alt,HOST_UNREACHABLE_ICON_ALT,sizeof(image_alt));
				history_detail_type=HISTORY_HOST_UNREACHABLE;
		                }
			else if(strstr(input_buffer,";RECOVERY") || strstr(input_buffer,";UP;")){
				strncpy(image,HOST_UP_ICON,sizeof(image));
				strncpy(image_alt,HOST_UP_ICON_ALT,sizeof(image_alt));
				history_detail_type=HISTORY_HOST_RECOVERY;
		                }
		        }

		/* host flapping alerts */
		else if(strstr(input_buffer,"HOST FLAPPING ALERT:")){
			
			if(display_flapping_alerts==FALSE)
				continue;
			
			history_type=HOST_FLAPPING_HISTORY;

			/* get host name */
			temp_buffer=my_strtok(input_buffer2,"]");
			temp_buffer=my_strtok(NULL,":");
			temp_buffer=my_strtok(NULL,";");
			strncpy(entry_host_name,(temp_buffer==NULL)?"":temp_buffer+1,sizeof(entry_host_name));
			entry_host_name[sizeof(entry_host_name)-1]='\x0';

			strncpy(image,FLAPPING_ICON,sizeof(image));

			if(strstr(input_buffer,";STARTED;"))
			        strncpy(image_alt,"Host started flapping",sizeof(image_alt));
			else if(strstr(input_buffer,";STOPPED;"))
			        strncpy(image_alt,"Host stopped flapping",sizeof(image_alt));
			else if(strstr(input_buffer,";DISABLED;"))
			        strncpy(image_alt,"Host flap detection disabled",sizeof(image_alt));
		        }

		/* host downtime alerts */
		else if(strstr(input_buffer,"HOST DOWNTIME ALERT:")){
			
			if(display_downtime_alerts==FALSE)
				continue;
			
			history_type=HOST_DOWNTIME_HISTORY;

			/* get host name */
			temp_buffer=my_strtok(input_buffer2,"]");
			temp_buffer=my_strtok(NULL,":");
			temp_buffer=my_strtok(NULL,";");
			strncpy(entry_host_name,(temp_buffer==NULL)?"":temp_buffer+1,sizeof(entry_host_name));
			entry_host_name[sizeof(entry_host_name)-1]='\x0';

			strncpy(image,SCHEDULED_DOWNTIME_ICON,sizeof(image));

			if(strstr(input_buffer,";STARTED;"))
			        strncpy(image_alt,"Host entered a period of scheduled downtime",sizeof(image_alt));
			else if(strstr(input_buffer,";STOPPED;"))
			        strncpy(image_alt,"Host exited from a period of scheduled downtime",sizeof(image_alt));
			else if(strstr(input_buffer,";CANCELLED;"))
			        strncpy(image_alt,"Host scheduled downtime has been cancelled",sizeof(image_alt));
		        }

		else if(display_system_messages==FALSE)
			continue;

		/* program start */
		else if(strstr(input_buffer," starting...")){
			strncpy(image,START_ICON,sizeof(image));
			strncpy(image_alt,START_ICON_ALT,sizeof(image_alt));
			system_message=TRUE;
		        }

		/* normal program termination */
		else if(strstr(input_buffer," shutting down...")){
			strncpy(image,STOP_ICON,sizeof(image));
			strncpy(image_alt,STOP_ICON_ALT,sizeof(image_alt));
			system_message=TRUE;
		        }

		/* abnormal program termination */
		else if(strstr(input_buffer,"Bailing out")){
			strncpy(image,STOP_ICON,sizeof(image));
			strncpy(image_alt,STOP_ICON_ALT,sizeof(image_alt));
			system_message=TRUE;
		        }

		/* program restart */
		else if(strstr(input_buffer," restarting...")){
			strncpy(image,RESTART_ICON,sizeof(image));
			strncpy(image_alt,RESTART_ICON_ALT,sizeof(image_alt));
			system_message=TRUE;
		        }

		image[sizeof(image)-1]='\x0';
		image_alt[sizeof(image_alt)-1]='\x0';

		/* get the timestamp */
		temp_buffer=strtok(input_buffer,"]");
		t=(temp_buffer==NULL)?0L:strtoul(temp_buffer+1,NULL,10);

		time_ptr=localtime(&t);
		strftime(current_message_date,sizeof(current_message_date),"%B %d, %Y %H:00\n",time_ptr);
		current_message_date[sizeof(current_message_date)-1]='\x0';

		get_time_string(&t,date_time,sizeof(date_time),SHORT_DATE_TIME);
		strip(date_time);

		temp_buffer=strtok(NULL,"\n");

		if(strcmp(image,"")){

			display_line=FALSE;

			if(system_message==TRUE)
				display_line=TRUE;

			else if(display_type==DISPLAY_HOSTS){

				if(history_type==HOST_HISTORY || history_type==SERVICE_HISTORY){
					sprintf(match1," HOST ALERT: %s;",host_name);
					sprintf(match2," SERVICE ALERT: %s;",host_name);
				        }
				else if(history_type==HOST_FLAPPING_HISTORY || history_type==SERVICE_FLAPPING_HISTORY){
					sprintf(match1," HOST FLAPPING ALERT: %s;",host_name);
					sprintf(match2," SERVICE FLAPPING ALERT: %s;",host_name);
				        }
				else if(history_type==HOST_DOWNTIME_HISTORY || history_type==SERVICE_DOWNTIME_HISTORY){
					sprintf(match1," HOST DOWNTIME ALERT: %s;",host_name);
					sprintf(match2," SERVICE DOWNTIME ALERT: %s;",host_name);
				        }

				if(show_all_hosts==TRUE)
					display_line=TRUE;
				else if(strstr(temp_buffer,match1))
					display_line=TRUE;
				else if(strstr(temp_buffer,match2))
					display_line=TRUE;

				if(display_line==TRUE){
					if(history_options==HISTORY_ALL)
						display_line=TRUE;
					else if(history_options==HISTORY_HOST_ALL && (history_type==HOST_HISTORY || history_type==HOST_FLAPPING_HISTORY || history_type==HOST_DOWNTIME_HISTORY))
						display_line=TRUE;
					else if(history_options==HISTORY_SERVICE_ALL && (history_type==SERVICE_HISTORY || history_type==SERVICE_FLAPPING_HISTORY || history_type==SERVICE_DOWNTIME_HISTORY))
						display_line=TRUE;
					else if((history_type==HOST_HISTORY || history_type==SERVICE_HISTORY) && (history_detail_type & history_options))
						display_line=TRUE;
					else 
						display_line=FALSE;
			                }

				/* check alert state types */
				if(display_line==TRUE && (history_type==HOST_HISTORY || history_type==SERVICE_HISTORY)){
					if(state_options==STATE_ALL)
						display_line=TRUE;
					else if((state_options & STATE_SOFT) && strstr(temp_buffer,";SOFT;"))
						display_line=TRUE;
					else if((state_options & STATE_HARD) && strstr(temp_buffer,";HARD;"))
						display_line=TRUE;
					else
						display_line=FALSE;
				        }
			        }

			else if(display_type==DISPLAY_SERVICES){

				if(history_type==SERVICE_HISTORY)
					sprintf(match1," SERVICE ALERT: %s;%s",host_name,svc_description);
				else if(history_type==SERVICE_FLAPPING_HISTORY)
					sprintf(match1," SERVICE FLAPPING ALERT: %s;%s",host_name,svc_description);
				else if(history_type==SERVICE_DOWNTIME_HISTORY)
					sprintf(match1," SERVICE DOWNTIME ALERT: %s;%s",host_name,svc_description);

				if(strstr(temp_buffer,match1))
					display_line=TRUE;

				/* check alert state type */
				if(display_line==TRUE && history_type==SERVICE_HISTORY){

					if(state_options==STATE_ALL)
						display_line=TRUE;
					else if((state_options & STATE_SOFT) && strstr(temp_buffer,";SOFT;"))
						display_line=TRUE;
					else if((state_options & STATE_HARD) && strstr(temp_buffer,";HARD;"))
						display_line=TRUE;
					else
						display_line=FALSE;
				        }
				else
					display_line=FALSE;
			        }


			/* make sure user is authorized to view this host or service information */
			if(system_message==FALSE){

				if(history_type==HOST_HISTORY || history_type==HOST_FLAPPING_HISTORY || history_type==HOST_DOWNTIME_HISTORY){
					temp_host=find_host(entry_host_name,NULL);
					if(is_authorized_for_host(temp_host,&current_authdata)==FALSE)
						display_line=FALSE;
					
				        }
				else{
					temp_service=find_service(entry_host_name,entry_service_desc,NULL);
					if(is_authorized_for_service(temp_service,&current_authdata)==FALSE)
						display_line=FALSE;
				        }
			        }
			
			/* display the entry if we should... */
			if(display_line==TRUE){

				if(strcmp(last_message_date,current_message_date)!=0 && display_timebreaks==TRUE){
					printf("<BR CLEAR='all'>\n");
					printf("<DIV CLASS='dateTimeBreak'>\n");
					printf("<table border=0 width=95%%><tr>");
					printf("<td width=40%%><hr width=100%%></td>");
					printf("<td align=center CLASS='dateTimeBreak'>%s</td>",current_message_date);
					printf("<td width=40%%><hr width=100%%></td>");
					printf("</tr></table>\n");
					printf("</DIV>\n");
					printf("<BR CLEAR='all'><DIV CLASS='logEntries'>\n");
					strncpy(last_message_date,current_message_date,sizeof(last_message_date));
					last_message_date[sizeof(last_message_date)-1]='\x0';
				        }

				if(display_frills==TRUE)
					printf("<img align='left' src='%s%s' alt='%s'>",url_images_path,image,image_alt);
				printf("[%s] %s<br clear='all'>\n",date_time,temp_buffer);
				found_line=TRUE;
			        }
		        }

                }

	printf("</P>\n");
	
	if(found_line==FALSE){
		printf("<HR>\n");
		printf("<P><DIV CLASS='warningMessage'>No history information was found ");
		if(display_type==DISPLAY_HOSTS)
			printf("%s",(show_all_hosts==TRUE)?"":"for this host ");
		else
			printf("for this this service ");
		printf("in %s log file</DIV></P>",(log_archive==0)?"the current":"this archived");
	        }

	printf("<HR>\n");

	if(use_lifo==TRUE)
		free_lifo_memory();
	else
		fclose(fp);

	return;
        }