/**************************************************************************
 *
 * CMD.C -  Nagios Command CGI
 *
 * Copyright (c) 1999-2002 Ethan Galstad (nagios@nagios.org)
 * Last Modified: 02-28-2002
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
#include "../common/downtime.h"

#include "cgiutils.h"
#include "getcgi.h"
#include "auth.h"

extern char main_config_file[MAX_FILENAME_LENGTH];
extern char command_file[MAX_FILENAME_LENGTH];
extern char comment_file[MAX_FILENAME_LENGTH];

extern char url_stylesheets_path[MAX_FILENAME_LENGTH];

extern int  nagios_process_state;

extern int  check_external_commands;

extern int  use_authentication;

extern scheduled_downtime *scheduled_downtime_list;
extern comment *comment_list;
extern host *host_list;



#define MAX_AUTHOR_LENGTH	64
#define MAX_COMMENT_LENGTH	1024

#define HTML_CONTENT   0
#define WML_CONTENT    1


char *host_name="";
char *hostgroup_name="";
char *service_desc="";
char *comment_author="";
char *comment_data="";

int comment_id=0;
int downtime_id=0;
int notification_delay=0;
int schedule_delay=0;
int persistent_comment=FALSE;
int send_notification=FALSE;
int force_check=FALSE;
int plugin_state=STATE_OK;
char plugin_output[MAX_INPUT_BUFFER]="";
time_t start_time=0L;
time_t end_time=0L;
int affect_host_and_services=FALSE;
int fixed=FALSE;
unsigned long duration=0L;

int command_type=CMD_NONE;
int command_mode=CMDMODE_REQUEST;

int content_type=HTML_CONTENT;

int display_header=TRUE;

authdata current_authdata;

void show_command_help(int);
void request_command_data(int);
void commit_command_data(int);
int commit_command(int);
int commit_hostgroup_command(int);
int write_command_to_file(char *);
void clean_comment_data(char *);

void document_header(int);
void document_footer(void);
int process_cgivars(void);

int time_to_string(time_t *,char *,int);
int string_to_time(char *,time_t *);



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
		if(content_type==WML_CONTENT)
			printf("<p>Error: Could not open CGI config file!</p>\n");
		else
			cgi_config_file_error(DEFAULT_CGI_CONFIG_FILE);
		document_footer();
		return ERROR;
	        }

	/* read the main configuration file */
	result=read_main_config_file(main_config_file);
	if(result==ERROR){
		document_header(FALSE);
		if(content_type==WML_CONTENT)
			printf("<p>Error: Could not open main config file!</p>\n");
		else
			main_config_file_error(main_config_file);
		document_footer();
		return ERROR;
	        }

	/* read all object configuration data */
	result=read_all_object_configuration_data(main_config_file,READ_HOSTGROUPS|READ_CONTACTGROUPS|READ_HOSTS|READ_SERVICES);
	if(result==ERROR){
		document_header(FALSE);
		if(content_type==WML_CONTENT)
			printf("<p>Error: Could not read object config data!</p>\n");
		else
			object_data_error();
		document_footer();
		return ERROR;
                }

	document_header(TRUE);

	/* get authentication information */
	get_authentication_information(&current_authdata);

	if(display_header==TRUE){

		/* begin top table */
		printf("<table border=0 width=100%%>\n");
		printf("<tr>\n");

		/* left column of the first row */
		printf("<td align=left valign=top width=33%%>\n");
		display_info_table("External Command Interface",FALSE,&current_authdata);
		printf("</td>\n");

		/* center column of the first row */
		printf("<td align=center valign=top width=33%%>\n");
		printf("</td>\n");

		/* right column of the first row */
		printf("<td align=right valign=bottom width=33%%>\n");

#ifdef CONTEXT_HELP
		if(command_mode==CMDMODE_COMMIT)
			display_context_help(CONTEXTHELP_CMD_COMMIT);
		else
			display_context_help(CONTEXTHELP_CMD_INPUT);
#endif

		printf("</td>\n");

		/* end of top table */
		printf("</tr>\n");
		printf("</table>\n");
	        }

	/* if no command was specified... */
	if(command_type==CMD_NONE){
		if(content_type==WML_CONTENT)
			printf("<p>Error: No command specified!</p>\n");
		else
			printf("<P><DIV CLASS='errorMessage'>Error: No command was specified</DIV></P>\n");
                }

	/* if this is the first request for a command, present option */
	else if(command_mode==CMDMODE_REQUEST)
		request_command_data(command_type);

	/* the user wants to commit the command */
	else if(command_mode==CMDMODE_COMMIT)
		commit_command_data(command_type);

	document_footer();

	/* free allocated memory */
	free_memory();
	free_object_data();

	return OK;
        }



void document_header(int use_stylesheet){

	if(content_type==WML_CONTENT){

		printf("Content-type: text/vnd.wap.wml\n\n");

		printf("<?xml version=\"1.0\"?>\n");
		printf("<!DOCTYPE wml PUBLIC \"-//WAPFORUM//DTD WML 1.1//EN\" \"http://www.wapforum.org/DTD/wml_1.1.xml\">\n");

		printf("<wml>\n");

		printf("<card id='card1' title='Command Results'>\n");
	        }

	else{

		printf("Content-type: text/html\r\n\r\n");

		printf("<html>\n");
		printf("<head>\n");
		printf("<title>\n");
		printf("External Command Interface\n");
		printf("</title>\n");

		if(use_stylesheet==TRUE)
			printf("<LINK REL='stylesheet' TYPE='text/css' HREF='%s%s'>\n",url_stylesheets_path,COMMAND_CSS);

		printf("</head>\n");

		printf("<body CLASS='cmd'>\n");

		/* include user SSI header */
		include_ssi_files(COMMAND_CGI,SSI_HEADER);
	        }

	return;
        }


void document_footer(void){

	if(content_type==WML_CONTENT){
		printf("</card>\n");
		printf("</wml>\n");
	        }

	else{

		/* include user SSI footer */
		include_ssi_files(COMMAND_CGI,SSI_FOOTER);

		printf("</body>\n");
		printf("</html>\n");
	        }

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

		/* we found the command type */
		else if(!strcmp(variables[x],"cmd_typ")){
			x++;
			if(variables[x]==NULL){
				error=TRUE;
				break;
			        }

			command_type=atoi(variables[x]);
		        }

		/* we found the command mode */
		else if(!strcmp(variables[x],"cmd_mod")){
			x++;
			if(variables[x]==NULL){
				error=TRUE;
				break;
			        }

			command_mode=atoi(variables[x]);
		        }

		/* we found the comment id */
		else if(!strcmp(variables[x],"com_id")){
			x++;
			if(variables[x]==NULL){
				error=TRUE;
				break;
			        }

			comment_id=atoi(variables[x]);
		        }

		/* we found the downtime id */
		else if(!strcmp(variables[x],"down_id")){
			x++;
			if(variables[x]==NULL){
				error=TRUE;
				break;
			        }

			downtime_id=atoi(variables[x]);
		        }

		/* we found the notification delay */
		else if(!strcmp(variables[x],"not_dly")){
			x++;
			if(variables[x]==NULL){
				error=TRUE;
				break;
			        }

			notification_delay=atoi(variables[x]);
		        }

		/* we found the schedule delay */
		else if(!strcmp(variables[x],"sched_dly")){
			x++;
			if(variables[x]==NULL){
				error=TRUE;
				break;
			        }

			schedule_delay=atoi(variables[x]);
		        }

		/* we found the comment author */
		else if(!strcmp(variables[x],"com_author")){
			x++;
			if(variables[x]==NULL){
				error=TRUE;
				break;
			        }

			comment_author=(char *)malloc(strlen(variables[x])+1);
			if(comment_author==NULL)
				comment_author="";
			else
				strcpy(comment_author,variables[x]);
			}

		/* we found the comment data */
		else if(!strcmp(variables[x],"com_data")){
			x++;
			if(variables[x]==NULL){
				error=TRUE;
				break;
			        }

			comment_data=(char *)malloc(strlen(variables[x])+1);
			if(comment_data==NULL)
				comment_data="";
			else
				strcpy(comment_data,variables[x]);
			}

		/* we found the host name */
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
			}

		/* we found the hostgroup name */
		else if(!strcmp(variables[x],"hostgroup")){
			x++;
			if(variables[x]==NULL){
				error=TRUE;
				break;
			        }

			hostgroup_name=(char *)malloc(strlen(variables[x])+1);
			if(hostgroup_name==NULL)
				hostgroup_name="";
			else
				strcpy(hostgroup_name,variables[x]);
			}

		/* we found the service name */
		else if(!strcmp(variables[x],"service")){
			x++;
			if(variables[x]==NULL){
				error=TRUE;
				break;
			        }

			service_desc=(char *)malloc(strlen(variables[x])+1);
			if(service_desc==NULL)
				service_desc="";
			else
				strcpy(service_desc,variables[x]);
			}

		/* we got the persistence option for a comment */
		else if(!strcmp(variables[x],"persistent"))
			persistent_comment=TRUE;

		/* we got the notification option for an acknowledgement */
		else if(!strcmp(variables[x],"send_notification"))
			send_notification=TRUE;

		/* we got the service check force option */
		else if(!strcmp(variables[x],"force_check"))
			force_check=TRUE;

		/* we got the option to affect host and all its services */
		else if(!strcmp(variables[x],"ahas"))
			affect_host_and_services=TRUE;

		/* we got the option for fixed downtime */
		else if(!strcmp(variables[x],"fixed"))
			fixed=TRUE;

		/* we found the plugin output */
		else if(!strcmp(variables[x],"plugin_output")){
			x++;
			if(variables[x]==NULL){
				error=TRUE;
				break;
			        }

			/* protect against buffer overflows */
			if(strlen(variables[x])>=MAX_INPUT_BUFFER-1){
				error=TRUE;
				break;
			        }
			else
				strcpy(plugin_output,variables[x]);
			}

		/* we found the plugin state */
		else if(!strcmp(variables[x],"plugin_state")){
			x++;
			if(variables[x]==NULL){
				error=TRUE;
				break;
			        }

			plugin_state=atoi(variables[x]);
		        }

		/* we found the hour duration */
		else if(!strcmp(variables[x],"hours")){
			x++;
			if(variables[x]==NULL){
				error=TRUE;
				break;
			        }

			if(atoi(variables[x])<0){
				error=TRUE;
				break;
			        }
			duration+=(unsigned long)(atoi(variables[x])*3600);
		        }

		/* we found the minute duration */
		else if(!strcmp(variables[x],"minutes")){
			x++;
			if(variables[x]==NULL){
				error=TRUE;
				break;
			        }

			if(atoi(variables[x])<0){
				error=TRUE;
				break;
			        }
			duration+=(unsigned long)(atoi(variables[x])*60);
		        }

		/* we found the start time */
		else if(!strcmp(variables[x],"start_time")){
			x++;
			if(variables[x]==NULL){
				error=TRUE;
				break;
			        }

			string_to_time(variables[x],&start_time);
		        }

		/* we found the end time */
		else if(!strcmp(variables[x],"end_time")){
			x++;
			if(variables[x]==NULL){
				error=TRUE;
				break;
			        }

			string_to_time(variables[x],&end_time);
		        }

		/* we found the content type argument */
		else if(!strcmp(variables[x],"content")){
			x++;
			if(variables[x]==NULL){
				error=TRUE;
				break;
			        }
			if(!strcmp(variables[x],"wml")){
				content_type=WML_CONTENT;
				display_header=FALSE;
			        }
			else
				content_type=HTML_CONTENT;
		        }
                }

	return error;
        }



void request_command_data(int cmd){
	time_t t;
	char buffer[MAX_INPUT_BUFFER];
	contact *temp_contact;


	/* get default name to use for comment author */
	temp_contact=find_contact(current_authdata.username,NULL);
	if(temp_contact!=NULL && temp_contact->alias!=NULL)
		comment_author=temp_contact->alias;
	else
		comment_author=current_authdata.username;


	printf("<P><DIV ALIGN=CENTER CLASS='cmdType'>You are requesting to ");

	switch(cmd){

	case CMD_ADD_HOST_COMMENT:
	case CMD_ADD_SVC_COMMENT:
		printf("add a %s comment",(cmd==CMD_ADD_HOST_COMMENT)?"host":"service");
		break;

	case CMD_DEL_HOST_COMMENT:
	case CMD_DEL_SVC_COMMENT:
		printf("delete a %s comment",(cmd==CMD_DEL_HOST_COMMENT)?"host":"service");
		break;
		
	case CMD_DELAY_HOST_NOTIFICATION:
	case CMD_DELAY_SVC_NOTIFICATION:
		printf("delay a %s notification",(cmd==CMD_DELAY_HOST_NOTIFICATION)?"host":"service");
		break;

	case CMD_DELAY_SVC_CHECK:
		printf("re-schedule the next check of a service");
		break;

	case CMD_IMMEDIATE_SVC_CHECK:
		printf("schedule an immediate service check");
		break;

	case CMD_ENABLE_SVC_CHECK:
	case CMD_DISABLE_SVC_CHECK:
		printf("%s checks of a particular service",(cmd==CMD_ENABLE_SVC_CHECK)?"enable":"disable");
		break;
		
	case CMD_ENABLE_NOTIFICATIONS:
	case CMD_DISABLE_NOTIFICATIONS:
		printf("%s notifications",(cmd==CMD_ENABLE_NOTIFICATIONS)?"enable":"disable");
		break;
		
	case CMD_SHUTDOWN_PROCESS:
	case CMD_RESTART_PROCESS:
		printf("%s the Nagios process",(cmd==CMD_SHUTDOWN_PROCESS)?"shutdown":"restart");
		break;

	case CMD_ENABLE_HOST_SVC_CHECKS:
	case CMD_DISABLE_HOST_SVC_CHECKS:
		printf("%s checks of all services on a host",(cmd==CMD_ENABLE_HOST_SVC_CHECKS)?"enable":"disable");
		break;

	case CMD_DELAY_HOST_SVC_CHECKS:
		printf("delay all service checks for a host");
		break;

	case CMD_IMMEDIATE_HOST_SVC_CHECKS:
		printf("schedule an immediate check of all services for a host");
		break;

	case CMD_DEL_ALL_HOST_COMMENTS:
	case CMD_DEL_ALL_SVC_COMMENTS:
		printf("delete all comments for a %s",(cmd==CMD_DEL_ALL_HOST_COMMENTS)?"host":"service");
		break;

	case CMD_ENABLE_SVC_NOTIFICATIONS:
	case CMD_DISABLE_SVC_NOTIFICATIONS:
		printf("%s notifications for a service",(cmd==CMD_ENABLE_SVC_NOTIFICATIONS)?"enable":"disable");
		break;

	case CMD_ENABLE_HOST_NOTIFICATIONS:
	case CMD_DISABLE_HOST_NOTIFICATIONS:
		printf("%s notifications for a host",(cmd==CMD_ENABLE_HOST_NOTIFICATIONS)?"enable":"disable");
		break;

	case CMD_ENABLE_ALL_NOTIFICATIONS_BEYOND_HOST:
	case CMD_DISABLE_ALL_NOTIFICATIONS_BEYOND_HOST:
		printf("%s notifications for all hosts and services beyond a host",(cmd==CMD_ENABLE_ALL_NOTIFICATIONS_BEYOND_HOST)?"enable":"disable");
		break;

	case CMD_ENABLE_HOST_SVC_NOTIFICATIONS:
	case CMD_DISABLE_HOST_SVC_NOTIFICATIONS:
		printf("%s notifications for all services on a host",(cmd==CMD_ENABLE_HOST_SVC_NOTIFICATIONS)?"enable":"disable");
		break;

	case CMD_ACKNOWLEDGE_HOST_PROBLEM:
	case CMD_ACKNOWLEDGE_SVC_PROBLEM:
		printf("acknowledge a %s problem",(cmd==CMD_ACKNOWLEDGE_HOST_PROBLEM)?"host":"service");
		break;

	case CMD_START_EXECUTING_SVC_CHECKS:
	case CMD_STOP_EXECUTING_SVC_CHECKS:
		printf("%s executing service checks",(cmd==CMD_START_EXECUTING_SVC_CHECKS)?"start":"stop");
		break;

	case CMD_START_ACCEPTING_PASSIVE_SVC_CHECKS:
	case CMD_STOP_ACCEPTING_PASSIVE_SVC_CHECKS:
		printf("%s accepting passive service checks",(cmd==CMD_START_ACCEPTING_PASSIVE_SVC_CHECKS)?"start":"stop");
		break;

	case CMD_ENABLE_PASSIVE_SVC_CHECKS:
	case CMD_DISABLE_PASSIVE_SVC_CHECKS:
		printf("%s accepting passive service checks for a particular service",(cmd==CMD_ENABLE_PASSIVE_SVC_CHECKS)?"start":"stop");
		break;

	case CMD_ENABLE_EVENT_HANDLERS:
	case CMD_DISABLE_EVENT_HANDLERS:
		printf("%s event handlers",(cmd==CMD_ENABLE_EVENT_HANDLERS)?"enable":"disable");
		break;

	case CMD_ENABLE_HOST_EVENT_HANDLER:
	case CMD_DISABLE_HOST_EVENT_HANDLER:
		printf("%s the event handler for a particular host",(cmd==CMD_ENABLE_HOST_EVENT_HANDLER)?"enable":"disable");
		break;

	case CMD_ENABLE_SVC_EVENT_HANDLER:
	case CMD_DISABLE_SVC_EVENT_HANDLER:
		printf("%s the event handler for a particular service",(cmd==CMD_ENABLE_SVC_EVENT_HANDLER)?"enable":"disable");
		break;

	case CMD_ENABLE_HOST_CHECK:
	case CMD_DISABLE_HOST_CHECK:
		printf("%s checks of a particular host",(cmd==CMD_ENABLE_HOST_CHECK)?"enable":"disable");
		break;

	case CMD_STOP_OBSESSING_OVER_SVC_CHECKS:
	case CMD_START_OBSESSING_OVER_SVC_CHECKS:
		printf("%s obsessing over service checks",(cmd==CMD_STOP_OBSESSING_OVER_SVC_CHECKS)?"stop":"start");
		break;

	case CMD_REMOVE_HOST_ACKNOWLEDGEMENT:
	case CMD_REMOVE_SVC_ACKNOWLEDGEMENT:
		printf("remove a %s acknowledgement",(cmd==CMD_REMOVE_HOST_ACKNOWLEDGEMENT)?"host":"service");
		break;

	case CMD_SCHEDULE_HOST_DOWNTIME:
	case CMD_SCHEDULE_SVC_DOWNTIME:
		printf("schedule downtime for a particular %s",(cmd==CMD_SCHEDULE_HOST_DOWNTIME)?"host":"service");
		break;

	case CMD_PROCESS_SERVICE_CHECK_RESULT:
		printf("submit a passive check result for a particular service");
		break;

	case CMD_ENABLE_HOST_FLAP_DETECTION:
	case CMD_DISABLE_HOST_FLAP_DETECTION:
		printf("%s flap detection for a particular host",(cmd==CMD_ENABLE_HOST_FLAP_DETECTION)?"enable":"disable");
		break;

	case CMD_ENABLE_SVC_FLAP_DETECTION:
	case CMD_DISABLE_SVC_FLAP_DETECTION:
		printf("%s flap detection for a particular service",(cmd==CMD_ENABLE_SVC_FLAP_DETECTION)?"enable":"disable");
		break;

	case CMD_ENABLE_FLAP_DETECTION:
	case CMD_DISABLE_FLAP_DETECTION:
		printf("%s flap detection for hosts and services",(cmd==CMD_ENABLE_FLAP_DETECTION)?"enable":"disable");
		break;

	case CMD_ENABLE_HOSTGROUP_SVC_NOTIFICATIONS:
	case CMD_DISABLE_HOSTGROUP_SVC_NOTIFICATIONS:
		printf("%s notifications for all services in a particular hostgroup",(cmd==CMD_ENABLE_HOSTGROUP_SVC_NOTIFICATIONS)?"enable":"disable");
		break;

	case CMD_ENABLE_HOSTGROUP_HOST_NOTIFICATIONS:
	case CMD_DISABLE_HOSTGROUP_HOST_NOTIFICATIONS:
		printf("%s notifications for all hosts in a particular hostgroup",(cmd==CMD_ENABLE_HOSTGROUP_HOST_NOTIFICATIONS)?"enable":"disable");
		break;

	case CMD_ENABLE_HOSTGROUP_SVC_CHECKS:
	case CMD_DISABLE_HOSTGROUP_SVC_CHECKS:
		printf("%s checks of all services in a particular hostgroup",(cmd==CMD_ENABLE_HOSTGROUP_SVC_CHECKS)?"enable":"disable");
		break;

	case CMD_DEL_HOST_DOWNTIME:
	case CMD_DEL_SVC_DOWNTIME:
		printf("cancel scheduled downtime for a particular %s",(cmd==CMD_DEL_HOST_DOWNTIME)?"host":"service");
		break;

	case CMD_ENABLE_FAILURE_PREDICTION:
	case CMD_DISABLE_FAILURE_PREDICTION:
		printf("%s failure prediction for hosts and service",(cmd==CMD_ENABLE_FAILURE_PREDICTION)?"enable":"disable");
		break;

	case CMD_ENABLE_PERFORMANCE_DATA:
	case CMD_DISABLE_PERFORMANCE_DATA:
		printf("%s performance data processing for hosts and services",(cmd==CMD_ENABLE_PERFORMANCE_DATA)?"enable":"disable");
		break;

	case CMD_SCHEDULE_HOSTGROUP_HOST_DOWNTIME:
		printf("schedule downtime for all hosts in a particular hostgroup");
		break;

	case CMD_SCHEDULE_HOSTGROUP_SVC_DOWNTIME:
		printf("schedule downtime for all services in a particular hostgroup");
		break;

	default:
		printf("execute an unknown command.  Shame on you!</DIV>");
		return;
	        }

	printf("</DIV></p>\n");

	printf("<p>\n");
	printf("<div align='center'>\n");

	printf("<table border=0 width=90%%>\n");
	printf("<tr>\n");
	printf("<td align=center valign=top>\n");

	printf("<DIV ALIGN=CENTER CLASS='optBoxTitle'>Command Options</DIV>\n");

	printf("<TABLE CELLSPACING=0 CELLPADDING=0 BORDER=1 CLASS='optBox'>\n");
	printf("<TR><TD CLASS='optBoxItem'>\n");
	printf("<TABLE CELLSPACING=0 CELLPADDING=0 CLASS='optBox'>\n");

	printf("<tr><td CLASS='optBoxItem'><form method='post' action='%s'></td><td><INPUT TYPE='HIDDEN' NAME='cmd_typ' VALUE='%d'><INPUT TYPE='HIDDEN' NAME='cmd_mod' VALUE='%d'></td></tr>\n",COMMAND_CGI,cmd,CMDMODE_COMMIT);

	switch(cmd){

	case CMD_ADD_HOST_COMMENT:
	case CMD_ACKNOWLEDGE_HOST_PROBLEM:
		printf("<tr><td CLASS='optBoxRequiredItem'>Host Name:</td><td><b>");
		printf("<INPUT TYPE='TEXT' NAME='host' VALUE='%s'>",host_name);
		printf("</b></td></tr>\n");
		if(cmd==CMD_ACKNOWLEDGE_HOST_PROBLEM){
			printf("<tr><td CLASS='optBoxItem'>Send Notification:</td><td><b>");
			printf("<INPUT TYPE='checkbox' NAME='send_notification' CHECKED>");
			printf("</b></td></tr>\n");
		        }
		printf("<tr><td CLASS='optBoxItem'>Persistent:</td><td><b>");
		printf("<INPUT TYPE='checkbox' NAME='persistent' CHECKED>");
		printf("</b></td></tr>\n");
		printf("<tr><td CLASS='optBoxRequiredItem'>Author (Your Name):</td><td><b>");
		printf("<INPUT TYPE'TEXT' NAME='com_author' VALUE='%s'>",comment_author);
		printf("</b></td></tr>\n");
		printf("<tr><td CLASS='optBoxRequiredItem'>Comment:</td><td><b>");
		printf("<INPUT TYPE='TEXT' NAME='com_data' VALUE='%s' SIZE=40>",comment_data);
		printf("</b></td></tr>\n");
		break;
		
	case CMD_ADD_SVC_COMMENT:
	case CMD_ACKNOWLEDGE_SVC_PROBLEM:
		printf("<tr><td CLASS='optBoxRequiredItem'>Host Name:</td><td><b>");
		printf("<INPUT TYPE='TEXT' NAME='host' VALUE='%s'>",host_name);
		printf("</b></td></tr>\n");
		printf("<tr><td CLASS='optBoxRequiredItem'>Service:</td><td><b>");
		printf("<INPUT TYPE='TEXT' NAME='service' VALUE='%s'>",service_desc);
		if(cmd==CMD_ACKNOWLEDGE_SVC_PROBLEM){
			printf("<tr><td CLASS='optBoxItem'>Send Notification:</td><td><b>");
			printf("<INPUT TYPE='checkbox' NAME='send_notification' CHECKED>");
			printf("</b></td></tr>\n");
		        }
		printf("<tr><td CLASS='optBoxItem'>Persistent:</td><td><b>");
		printf("<INPUT TYPE='checkbox' NAME='persistent' CHECKED>");
		printf("</b></td></tr>\n");
		printf("<tr><td CLASS='optBoxRequiredItem'>Author (Your Name):</td><td><b>");
		printf("<INPUT TYPE='TEXT' NAME='com_author' VALUE='%s'>",comment_author);
		printf("</b></td></tr>\n");
		printf("<tr><td CLASS='optBoxRequiredItem'>Comment:</td><td><b>");
		printf("<INPUT TYPE='TEXT' NAME='com_data' VALUE='%s' SIZE=40>",comment_data);
		printf("</b></td></tr>\n");
		break;

	case CMD_DEL_HOST_COMMENT:
	case CMD_DEL_SVC_COMMENT:
		printf("<tr><td CLASS='optBoxRequiredItem'>Comment ID:</td><td><b>");
		printf("<INPUT TYPE='TEXT' NAME='com_id' VALUE='%d'>",comment_id);
		printf("</b></td></tr>\n");
		break;
		
	case CMD_DELAY_HOST_NOTIFICATION:
		printf("<tr><td CLASS='optBoxRequiredItem'>Host Name:</td><td><b>");
		printf("<INPUT TYPE='TEXT' NAME='host' VALUE='%s'>",host_name);
		printf("</b></td></tr>\n");
		printf("<tr><td CLASS='optBoxRequiredItem'>Notification Delay (minutes from now):</td><td><b>");
		printf("<INPUT TYPE='TEXT' NAME='not_dly' VALUE='%d'>",notification_delay);
		printf("</b></td></tr>\n");
		break;

	case CMD_DELAY_SVC_NOTIFICATION:
		printf("<tr><td CLASS='optBoxRequiredItem'>Host Name:</td><td><b>");
		printf("<INPUT TYPE='TEXT' NAME='host' VALUE='%s'>",host_name);
		printf("</b></td></tr>\n");
		printf("<tr><td CLASS='optBoxRequiredItem'>Service:</td><td><b>");
		printf("<INPUT TYPE='TEXT' NAME='service' VALUE='%s'>",service_desc);
		printf("<tr><td CLASS='optBoxRequiredItem'>Notification Delay (minutes from now):</td><td><b>");
		printf("<INPUT TYPE='TEXT' NAME='not_dly' VALUE='%d'>",notification_delay);
		printf("</b></td></tr>\n");
		break;

	case CMD_IMMEDIATE_SVC_CHECK:
	case CMD_DELAY_SVC_CHECK:
		printf("<tr><td CLASS='optBoxRequiredItem'>Host Name:</td><td><b>");
		printf("<INPUT TYPE='TEXT' NAME='host' VALUE='%s'>",host_name);
		printf("</b></td></tr>\n");
		printf("<tr><td CLASS='optBoxRequiredItem'>Service:</td><td><b>");
		printf("<INPUT TYPE='TEXT' NAME='service' VALUE='%s'>",service_desc);
		if(cmd!=CMD_IMMEDIATE_SVC_CHECK){
			time(&t);
			time_to_string(&t,buffer,sizeof(buffer)-1);
			printf("<tr><td CLASS='optBoxRequiredItem'>Check Time:</td><td><b>");
			printf("<INPUT TYPE='TEXT' NAME='start_time' VALUE='%s'>",buffer);
			printf("</b></td></tr>\n");
		        }
		printf("<tr><td CLASS='optBoxItem'>Force Check:</td><td><b>");
		printf("<INPUT TYPE='checkbox' NAME='force_check' CHECKED>");
		printf("</b></td></tr>\n");
		break;

	case CMD_IMMEDIATE_HOST_SVC_CHECKS:
	case CMD_DELAY_HOST_SVC_CHECKS:
		printf("<tr><td CLASS='optBoxRequiredItem'>Host Name:</td><td><b>");
		printf("<INPUT TYPE='TEXT' NAME='host' VALUE='%s'>",host_name);
		printf("</b></td></tr>\n");
		if(cmd!=CMD_IMMEDIATE_HOST_SVC_CHECKS){
			printf("<tr><td CLASS='optBoxRequiredItem'>Check Delay (minutes from now):</td><td><b>");
			printf("<INPUT TYPE='TEXT' NAME='sched_dly' VALUE='%d'>",schedule_delay);
			printf("</b></td></tr>\n");
		        }
		printf("<tr><td CLASS='optBoxItem'>Force Check:</td><td><b>");
		printf("<INPUT TYPE='checkbox' NAME='force_check'>");
		printf("</b></td></tr>\n");
		break;

	case CMD_ENABLE_SVC_CHECK:
	case CMD_DISABLE_SVC_CHECK:
	case CMD_DEL_ALL_SVC_COMMENTS:
	case CMD_ENABLE_SVC_NOTIFICATIONS:
	case CMD_DISABLE_SVC_NOTIFICATIONS:
	case CMD_ENABLE_PASSIVE_SVC_CHECKS:
	case CMD_DISABLE_PASSIVE_SVC_CHECKS:
	case CMD_ENABLE_SVC_EVENT_HANDLER:
	case CMD_DISABLE_SVC_EVENT_HANDLER:
	case CMD_REMOVE_SVC_ACKNOWLEDGEMENT:
	case CMD_ENABLE_SVC_FLAP_DETECTION:
	case CMD_DISABLE_SVC_FLAP_DETECTION:
		printf("<tr><td CLASS='optBoxRequiredItem'>Host Name:</td><td><b>");
		printf("<INPUT TYPE='TEXT' NAME='host' VALUE='%s'>",host_name);
		printf("</b></td></tr>\n");
		printf("<tr><td CLASS='optBoxRequiredItem'>Service:</td><td><b>");
		printf("<INPUT TYPE='TEXT' NAME='service' VALUE='%s'>",service_desc);
		printf("</b></td></tr>\n");
		break;
		
	case CMD_ENABLE_HOST_SVC_CHECKS:
	case CMD_DISABLE_HOST_SVC_CHECKS:
	case CMD_DEL_ALL_HOST_COMMENTS:
	case CMD_ENABLE_HOST_NOTIFICATIONS:
	case CMD_DISABLE_HOST_NOTIFICATIONS:
	case CMD_ENABLE_ALL_NOTIFICATIONS_BEYOND_HOST:
	case CMD_DISABLE_ALL_NOTIFICATIONS_BEYOND_HOST:
	case CMD_ENABLE_HOST_SVC_NOTIFICATIONS:
	case CMD_DISABLE_HOST_SVC_NOTIFICATIONS:
	case CMD_ENABLE_HOST_EVENT_HANDLER:
	case CMD_DISABLE_HOST_EVENT_HANDLER:
	case CMD_ENABLE_HOST_CHECK:
	case CMD_DISABLE_HOST_CHECK:
	case CMD_REMOVE_HOST_ACKNOWLEDGEMENT:
	case CMD_ENABLE_HOST_FLAP_DETECTION:
	case CMD_DISABLE_HOST_FLAP_DETECTION:
		printf("<tr><td CLASS='optBoxRequiredItem'>Host Name:</td><td><b>");
		printf("<INPUT TYPE='TEXT' NAME='host' VALUE='%s'>",host_name);
		printf("</b></td></tr>\n");
		if(cmd==CMD_ENABLE_HOST_SVC_CHECKS || cmd==CMD_DISABLE_HOST_SVC_CHECKS || cmd==CMD_ENABLE_HOST_SVC_NOTIFICATIONS || cmd==CMD_DISABLE_HOST_SVC_NOTIFICATIONS){
			printf("<tr><td CLASS='optBoxItem'>%s For Host Too:</td><td><b>",(cmd==CMD_ENABLE_HOST_SVC_CHECKS || cmd==CMD_ENABLE_HOST_SVC_NOTIFICATIONS)?"Enable":"Disable");
			printf("<INPUT TYPE='checkbox' NAME='ahas'>");
			printf("</b></td></tr>\n");
		        }
		break;

	case CMD_ENABLE_NOTIFICATIONS:
	case CMD_DISABLE_NOTIFICATIONS:
	case CMD_SHUTDOWN_PROCESS:
	case CMD_RESTART_PROCESS:
	case CMD_START_EXECUTING_SVC_CHECKS:
	case CMD_STOP_EXECUTING_SVC_CHECKS:
	case CMD_START_ACCEPTING_PASSIVE_SVC_CHECKS:
	case CMD_STOP_ACCEPTING_PASSIVE_SVC_CHECKS:
	case CMD_ENABLE_EVENT_HANDLERS:
	case CMD_DISABLE_EVENT_HANDLERS:
	case CMD_START_OBSESSING_OVER_SVC_CHECKS:
	case CMD_STOP_OBSESSING_OVER_SVC_CHECKS:
	case CMD_ENABLE_FLAP_DETECTION:
	case CMD_DISABLE_FLAP_DETECTION:
	case CMD_ENABLE_FAILURE_PREDICTION:
	case CMD_DISABLE_FAILURE_PREDICTION:
	case CMD_ENABLE_PERFORMANCE_DATA:
	case CMD_DISABLE_PERFORMANCE_DATA:
		printf("<tr><td CLASS='optBoxItem' colspan=2>There are no options for this command.<br>Click the 'Commit' button to submit the command.</td></tr>");
		break;
		
	case CMD_PROCESS_SERVICE_CHECK_RESULT:
		printf("<tr><td CLASS='optBoxRequiredItem'>Host Name:</td><td><b>");
		printf("<INPUT TYPE='TEXT' NAME='host' VALUE='%s'>",host_name);
		printf("</b></td></tr>\n");
		printf("<tr><td CLASS='optBoxRequiredItem'>Service:</td><td><b>");
		printf("<INPUT TYPE='TEXT' NAME='service' VALUE='%s'>",service_desc);
		printf("</b></td></tr>\n");
		printf("<tr><td CLASS='optBoxRequiredItem'>Check Result:</td><td><b>");
		printf("<SELECT NAME='plugin_state'>");
		printf("<OPTION VALUE=%d SELECTED>OK\n",STATE_OK);
		printf("<OPTION VALUE=%d>WARNING\n",STATE_WARNING);
		printf("<OPTION VALUE=%d>UNKNOWN\n",STATE_UNKNOWN);
		printf("<OPTION VALUE=%d>CRITICAL\n",STATE_CRITICAL);
		printf("</SELECT>\n");
		printf("</b></td></tr>\n");
		printf("<tr><td CLASS='optBoxRequiredItem'>Check Output:</td><td><b>");
		printf("<INPUT TYPE='TEXT' NAME='plugin_output' VALUE=''>");
		printf("</b></td></tr>\n");
		break;
		
	case CMD_SCHEDULE_HOST_DOWNTIME:
	case CMD_SCHEDULE_SVC_DOWNTIME:

		printf("<tr><td CLASS='optBoxRequiredItem'>Host Name:</td><td><b>");
		printf("<INPUT TYPE='TEXT' NAME='host' VALUE='%s'>",host_name);
		printf("</b></td></tr>\n");
		if(cmd==CMD_SCHEDULE_SVC_DOWNTIME){
			printf("<tr><td CLASS='optBoxRequiredItem'>Service:</td><td><b>");
			printf("<INPUT TYPE='TEXT' NAME='service' VALUE='%s'>",service_desc);
		        }
		printf("<tr><td CLASS='optBoxRequiredItem'>Author (Your Name):</td><td><b>");
		printf("<INPUT TYPE='TEXT' NAME='com_author' VALUE='%s'>",comment_author);
		printf("</b></td></tr>\n");
		printf("<tr><td CLASS='optBoxRequiredItem'>Comment:</td><td><b>");
		printf("<INPUT TYPE='TEXT' NAME='com_data' VALUE='%s' SIZE=40>",comment_data);
		printf("</b></td></tr>\n");
		time(&t);
		time_to_string(&t,buffer,sizeof(buffer)-1);
		printf("<tr><td CLASS='optBoxRequiredItem'>Start Time:</td><td><b>");
		printf("<INPUT TYPE='TEXT' NAME='start_time' VALUE='%s'>",buffer);
		printf("</b></td></tr>\n");
		t+=(unsigned long)7200;
		time_to_string(&t,buffer,sizeof(buffer)-1);
		printf("<tr><td CLASS='optBoxRequiredItem'>End Time:</td><td><b>");
		printf("<INPUT TYPE='TEXT' NAME='end_time' VALUE='%s'>",buffer);
		printf("</b></td></tr>\n");
		printf("<tr><td CLASS='optBoxItem'>Fixed:</td><td><b>");
		printf("<INPUT TYPE='checkbox' NAME='fixed' CHECKED>");
		printf("</b></td></tr>\n");

		printf("<tr><td CLASS='optBoxItem'>Duration:</td><td>");
		printf("<table border=0><tr>\n");
		printf("<td align=right><INPUT TYPE='TEXT' NAME='hours' VALUE='2' SIZE=2 MAXLENGTH=2></td>\n");
		printf("<td align=left>Hours</td>\n");
		printf("<td align=right><INPUT TYPE='TEXT' NAME='minutes' VALUE='0' SIZE=2 MAXLENGTH=2></td>\n");
		printf("<td align=left>Minutes</td>\n");
		printf("</tr></table>\n");
		printf("</td></tr>\n");
		break;

	case CMD_ENABLE_HOSTGROUP_SVC_NOTIFICATIONS:
	case CMD_DISABLE_HOSTGROUP_SVC_NOTIFICATIONS:
	case CMD_ENABLE_HOSTGROUP_HOST_NOTIFICATIONS:
	case CMD_DISABLE_HOSTGROUP_HOST_NOTIFICATIONS:
	case CMD_ENABLE_HOSTGROUP_SVC_CHECKS:
	case CMD_DISABLE_HOSTGROUP_SVC_CHECKS:
		printf("<tr><td CLASS='optBoxRequiredItem'>Hostgroup Name:</td><td><b>");
		printf("<INPUT TYPE='TEXT' NAME='hostgroup' VALUE='%s'>",hostgroup_name);
		printf("</b></td></tr>\n");
		if(cmd==CMD_ENABLE_HOSTGROUP_SVC_CHECKS || cmd==CMD_DISABLE_HOSTGROUP_SVC_CHECKS || cmd==CMD_ENABLE_HOSTGROUP_SVC_NOTIFICATIONS || cmd==CMD_DISABLE_HOSTGROUP_SVC_NOTIFICATIONS){
			printf("<tr><td CLASS='optBoxItem'>%s For Hosts Too:</td><td><b>",(cmd==CMD_ENABLE_HOSTGROUP_SVC_CHECKS || cmd==CMD_ENABLE_HOSTGROUP_SVC_NOTIFICATIONS)?"Enable":"Disable");
			printf("<INPUT TYPE='checkbox' NAME='ahas'>");
			printf("</b></td></tr>\n");
		        }
		break;
		
	case CMD_DEL_HOST_DOWNTIME:
	case CMD_DEL_SVC_DOWNTIME:
		printf("<tr><td CLASS='optBoxRequiredItem'>Scheduled Downtime ID:</td><td><b>");
		printf("<INPUT TYPE='TEXT' NAME='down_id' VALUE='%d'>",downtime_id);
		printf("</b></td></tr>\n");
		break;


	case CMD_SCHEDULE_HOSTGROUP_HOST_DOWNTIME:
	case CMD_SCHEDULE_HOSTGROUP_SVC_DOWNTIME:

		printf("<tr><td CLASS='optBoxRequiredItem'>Hostgroup Name:</td><td><b>");
		printf("<INPUT TYPE='TEXT' NAME='hostgroup' VALUE='%s'>",hostgroup_name);
		printf("</b></td></tr>\n");
		printf("<tr><td CLASS='optBoxRequiredItem'>Author (Your Name):</td><td><b>");
		printf("<INPUT TYPE='TEXT' NAME='com_author' VALUE='%s'>",comment_author);
		printf("</b></td></tr>\n");
		printf("<tr><td CLASS='optBoxRequiredItem'>Comment:</td><td><b>");
		printf("<INPUT TYPE='TEXT' NAME='com_data' VALUE='%s' SIZE=40>",comment_data);
		printf("</b></td></tr>\n");
		time(&t);
		time_to_string(&t,buffer,sizeof(buffer)-1);
		printf("<tr><td CLASS='optBoxRequiredItem'>Start Time:</td><td><b>");
		printf("<INPUT TYPE='TEXT' NAME='start_time' VALUE='%s'>",buffer);
		printf("</b></td></tr>\n");
		t+=(unsigned long)7200;
		time_to_string(&t,buffer,sizeof(buffer)-1);
		printf("<tr><td CLASS='optBoxRequiredItem'>End Time:</td><td><b>");
		printf("<INPUT TYPE='TEXT' NAME='end_time' VALUE='%s'>",buffer);
		printf("</b></td></tr>\n");
		printf("<tr><td CLASS='optBoxItem'>Fixed:</td><td><b>");
		printf("<INPUT TYPE='checkbox' NAME='fixed' CHECKED>");
		printf("</b></td></tr>\n");

		printf("<tr><td CLASS='optBoxItem'>Duration:</td><td>");
		printf("<table border=0><tr>\n");
		printf("<td align=right><INPUT TYPE='TEXT' NAME='hours' VALUE='2' SIZE=2 MAXLENGTH=2></td>\n");
		printf("<td align=left>Hours</td>\n");
		printf("<td align=right><INPUT TYPE='TEXT' NAME='minutes' VALUE='0' SIZE=2 MAXLENGTH=2></td>\n");
		printf("<td align=left>Minutes</td>\n");
		printf("</tr></table>\n");
		printf("</td></tr>\n");
		if(cmd==CMD_SCHEDULE_HOSTGROUP_SVC_DOWNTIME){
			printf("<tr><td CLASS='optBoxItem'>Schedule Downtime For Hosts Too:</td><td><b>");
			printf("<INPUT TYPE='checkbox' NAME='ahas'>");
			printf("</b></td></tr>\n");
		        }
		break;

	default:
		printf("<tr><td CLASS='optBoxItem'>This should not be happening... :-(</td><td></td></tr>\n");
	        }


	printf("<tr><td CLASS='optBoxItem' COLSPAN=2></td></tr>\n");
	printf("<tr><td CLASS='optBoxItem'></td><td CLASS='optBoxItem'><INPUT TYPE='submit' NAME='btnSubmit' VALUE='Commit'> <INPUT TYPE='reset' VALUE='Reset'></FORM></td></tr>\n");

	printf("</table>\n");

	printf("</td>\n");
	printf("</tr>\n");
	printf("</table>\n");

	printf("</td>\n");
	printf("<td align=center valign=top width=50%%>\n");

	/* show information about the command... */
	show_command_help(cmd);
	
	printf("</td>\n");
	printf("</tr>\n");
	printf("</table>\n");

	printf("</div>\n");
	printf("</p>\n");


	printf("<P><DIV CLASS='infoMessage'>Please enter all required information before committing the command.<br>Required fields are marked in red.<br>Failure to supply all required values will result in an error.</DIV></P>");

	return;
        }


void commit_command_data(int cmd){
	int error=FALSE;
	int result=OK;
	int authorized=FALSE;
	service *temp_service;
	host *temp_host;
	hostgroup *temp_hostgroup;
	comment *temp_comment;
	scheduled_downtime *temp_downtime;


	/* get authentication information */
	get_authentication_information(&current_authdata);

	switch(cmd){
	case CMD_ADD_HOST_COMMENT:
	case CMD_ACKNOWLEDGE_HOST_PROBLEM:

		/* make sure we have some host name, author name, and comment data... */
		if(!strcmp(host_name,"") || !strcmp(comment_author,"") || !strcmp(comment_data,""))
			error=TRUE;

		/* clean up the comment data */
		clean_comment_data(comment_author);
		clean_comment_data(comment_data);

		/* see if the user is authorized to issue a command... */
		temp_host=find_host(host_name,NULL);
		if(is_authorized_for_host_commands(temp_host,&current_authdata)==TRUE)
			authorized=TRUE;
		break;
		
	case CMD_ADD_SVC_COMMENT:
	case CMD_ACKNOWLEDGE_SVC_PROBLEM:

		/* make sure we have some host name and service description, author name, and comment data... */
		if(!strcmp(host_name,"") || !strcmp(service_desc,"") || !strcmp(comment_author,"") || !strcmp(comment_data,""))
			error=TRUE;

		/* clean up the comment data */
		clean_comment_data(comment_author);
		clean_comment_data(comment_data);

		/* see if the user is authorized to issue a command... */
		temp_service=find_service(host_name,service_desc,NULL);
		if(is_authorized_for_service_commands(temp_service,&current_authdata)==TRUE)
			authorized=TRUE;
		break;

	case CMD_DEL_HOST_COMMENT:
	case CMD_DEL_SVC_COMMENT:

		/* check the sanity of the comment id */
		if(comment_id<=0)
			error=TRUE;

		/* read comments */
		read_comment_data(DEFAULT_CGI_CONFIG_FILE);

		/* find the comment */
		if(cmd==CMD_DEL_HOST_COMMENT)
			temp_comment=find_host_comment(comment_id);
		else
			temp_comment=find_service_comment(comment_id);

		/* see if the user is authorized to issue a command... */
		if(cmd==CMD_DEL_HOST_COMMENT && temp_comment!=NULL){
			temp_host=find_host(temp_comment->host_name,NULL);
			if(is_authorized_for_host_commands(temp_host,&current_authdata)==TRUE)
				authorized=TRUE;
		        }
		if(cmd==CMD_DEL_SVC_COMMENT && temp_comment!=NULL){
			temp_service=find_service(temp_comment->host_name,temp_comment->service_description,NULL);
			if(is_authorized_for_service_commands(temp_service,&current_authdata)==TRUE)
				authorized=TRUE;
		        }

		/* free comment data */
		free_comment_data();

		break;
		
	case CMD_DEL_HOST_DOWNTIME:
	case CMD_DEL_SVC_DOWNTIME:

		/* check the sanity of the downtime id */
		if(downtime_id<=0)
			error=TRUE;

		/* read scheduled downtime */
		read_downtime_data(DEFAULT_CGI_CONFIG_FILE);

		/* find the downtime entry */
		if(cmd==CMD_DEL_HOST_DOWNTIME)
			temp_downtime=find_host_downtime(downtime_id);
		else
			temp_downtime=find_service_downtime(downtime_id);

		/* see if the user is authorized to issue a command... */
		if(cmd==CMD_DEL_HOST_DOWNTIME && temp_downtime!=NULL){
			temp_host=find_host(temp_downtime->host_name,NULL);
			if(is_authorized_for_host_commands(temp_host,&current_authdata)==TRUE)
				authorized=TRUE;
		        }
		if(cmd==CMD_DEL_SVC_DOWNTIME && temp_downtime!=NULL){
			temp_service=find_service(temp_downtime->host_name,temp_downtime->service_description,NULL);
			if(is_authorized_for_service_commands(temp_service,&current_authdata)==TRUE)
				authorized=TRUE;
		        }

		/* free downtime data */
		free_downtime_data();

		break;
		
	case CMD_DELAY_SVC_CHECK:
	case CMD_IMMEDIATE_SVC_CHECK:
	case CMD_ENABLE_SVC_CHECK:
	case CMD_DISABLE_SVC_CHECK:
	case CMD_DEL_ALL_SVC_COMMENTS:
	case CMD_ENABLE_SVC_NOTIFICATIONS:
	case CMD_DISABLE_SVC_NOTIFICATIONS:
	case CMD_ENABLE_PASSIVE_SVC_CHECKS:
	case CMD_DISABLE_PASSIVE_SVC_CHECKS:
	case CMD_ENABLE_SVC_EVENT_HANDLER:
	case CMD_DISABLE_SVC_EVENT_HANDLER:
	case CMD_REMOVE_SVC_ACKNOWLEDGEMENT:
	case CMD_PROCESS_SERVICE_CHECK_RESULT:
	case CMD_SCHEDULE_SVC_DOWNTIME:
	case CMD_DELAY_SVC_NOTIFICATION:
	case CMD_ENABLE_SVC_FLAP_DETECTION:
	case CMD_DISABLE_SVC_FLAP_DETECTION:

		/* make sure we have some host name and service description... */
		if(!strcmp(host_name,"") || !strcmp(service_desc,""))
			error=TRUE;

		/* make sure we have author name and comment data... */
		if(cmd==CMD_SCHEDULE_SVC_DOWNTIME && (!strcmp(comment_author,"") || !strcmp(comment_data,"")))
			error=TRUE;

		/* see if the user is authorized to issue a command... */
		temp_service=find_service(host_name,service_desc,NULL);
		if(is_authorized_for_service_commands(temp_service,&current_authdata)==TRUE)
			authorized=TRUE;

		/* make sure we have passive check info (if necessary) */
		if(cmd==CMD_PROCESS_SERVICE_CHECK_RESULT && !strcmp(plugin_output,""))
			error=TRUE;

		/* make sure we have a notification delay (if necessary) */
		if(cmd==CMD_DELAY_SVC_NOTIFICATION && notification_delay<=0)
			error=TRUE;

		/* clean up the comment data if scheduling downtime */
		if(cmd==CMD_SCHEDULE_SVC_DOWNTIME){
			clean_comment_data(comment_author);
			clean_comment_data(comment_data);
		        }

		/* make sure we have start/end times for downtime (if necessary) */
		if(cmd==CMD_SCHEDULE_SVC_DOWNTIME && (start_time==(time_t)0 || end_time==(time_t)0 || end_time<start_time))
			error=TRUE;

		break;
		
	case CMD_ENABLE_NOTIFICATIONS:
	case CMD_DISABLE_NOTIFICATIONS:
	case CMD_SHUTDOWN_PROCESS:
	case CMD_RESTART_PROCESS:
	case CMD_START_EXECUTING_SVC_CHECKS:
	case CMD_STOP_EXECUTING_SVC_CHECKS:
	case CMD_START_ACCEPTING_PASSIVE_SVC_CHECKS:
	case CMD_STOP_ACCEPTING_PASSIVE_SVC_CHECKS:
	case CMD_ENABLE_EVENT_HANDLERS:
	case CMD_DISABLE_EVENT_HANDLERS:
	case CMD_START_OBSESSING_OVER_SVC_CHECKS:
	case CMD_STOP_OBSESSING_OVER_SVC_CHECKS:
	case CMD_ENABLE_FLAP_DETECTION:
	case CMD_DISABLE_FLAP_DETECTION:
	case CMD_ENABLE_FAILURE_PREDICTION:
	case CMD_DISABLE_FAILURE_PREDICTION:
	case CMD_ENABLE_PERFORMANCE_DATA:
	case CMD_DISABLE_PERFORMANCE_DATA:

		/* see if the user is authorized to issue a command... */
		if(is_authorized_for_system_commands(&current_authdata)==TRUE)
			authorized=TRUE;
		break;
		
	case CMD_ENABLE_HOST_SVC_CHECKS:
	case CMD_DISABLE_HOST_SVC_CHECKS:
	case CMD_DEL_ALL_HOST_COMMENTS:
	case CMD_IMMEDIATE_HOST_SVC_CHECKS:
	case CMD_DELAY_HOST_SVC_CHECKS:
	case CMD_ENABLE_HOST_NOTIFICATIONS:
	case CMD_DISABLE_HOST_NOTIFICATIONS:
	case CMD_ENABLE_ALL_NOTIFICATIONS_BEYOND_HOST:
	case CMD_DISABLE_ALL_NOTIFICATIONS_BEYOND_HOST:
	case CMD_ENABLE_HOST_SVC_NOTIFICATIONS:
	case CMD_DISABLE_HOST_SVC_NOTIFICATIONS:
	case CMD_ENABLE_HOST_EVENT_HANDLER:
	case CMD_DISABLE_HOST_EVENT_HANDLER:
	case CMD_ENABLE_HOST_CHECK:
	case CMD_DISABLE_HOST_CHECK:
	case CMD_REMOVE_HOST_ACKNOWLEDGEMENT:
	case CMD_SCHEDULE_HOST_DOWNTIME:
	case CMD_DELAY_HOST_NOTIFICATION:
	case CMD_ENABLE_HOST_FLAP_DETECTION:
	case CMD_DISABLE_HOST_FLAP_DETECTION:

		/* make sure we have some host name... */
		if(!strcmp(host_name,""))
			error=TRUE;

		/* make sure we have author name and comment data... */
		if(cmd==CMD_SCHEDULE_HOST_DOWNTIME && (!strcmp(comment_author,"") || !strcmp(comment_data,"")))
			error=TRUE;

		/* see if the user is authorized to issue a command... */
		temp_host=find_host(host_name,NULL);
		if(is_authorized_for_host_commands(temp_host,&current_authdata)==TRUE)
			authorized=TRUE;

		/* clean up the comment data if scheduling downtime */
		if(cmd==CMD_SCHEDULE_HOST_DOWNTIME){
			clean_comment_data(comment_author);
			clean_comment_data(comment_data);
		        }

		/* make sure we have a notification delay (if necessary) */
		if(cmd==CMD_DELAY_HOST_NOTIFICATION && notification_delay<=0)
			error=TRUE;

		/* make sure we have start/end times for downtime (if necessary) */
		if(cmd==CMD_SCHEDULE_HOST_DOWNTIME && (start_time==(time_t)0 || end_time==(time_t)0 || start_time>end_time))
			error=TRUE;

		break;

	case CMD_ENABLE_HOSTGROUP_SVC_NOTIFICATIONS:
	case CMD_DISABLE_HOSTGROUP_SVC_NOTIFICATIONS:
	case CMD_ENABLE_HOSTGROUP_HOST_NOTIFICATIONS:
	case CMD_DISABLE_HOSTGROUP_HOST_NOTIFICATIONS:
	case CMD_ENABLE_HOSTGROUP_SVC_CHECKS:
	case CMD_DISABLE_HOSTGROUP_SVC_CHECKS:
	case CMD_SCHEDULE_HOSTGROUP_HOST_DOWNTIME:
	case CMD_SCHEDULE_HOSTGROUP_SVC_DOWNTIME:

		/* make sure we have some hostgroup name... */
		if(!strcmp(hostgroup_name,""))
			error=TRUE;

		/* make sure we have author and comment data */
		if((cmd==CMD_SCHEDULE_HOSTGROUP_HOST_DOWNTIME || cmd==CMD_SCHEDULE_HOSTGROUP_SVC_DOWNTIME) && (!strcmp(comment_author,"") || !strcmp(comment_data,"")))
			error=TRUE;

		/* make sure we have start/end times for downtime */
		if((cmd==CMD_SCHEDULE_HOSTGROUP_HOST_DOWNTIME || cmd==CMD_SCHEDULE_HOSTGROUP_SVC_DOWNTIME) && (start_time==(time_t)0 || end_time==(time_t)0 || start_time>end_time))
			error=TRUE;

		/* see if the user is authorized to issue a command... */
		temp_hostgroup=find_hostgroup(hostgroup_name,NULL);
		if(is_authorized_for_hostgroup(temp_hostgroup,&current_authdata)==TRUE)
			authorized=TRUE;

		/* clean up the comment data if scheduling downtime */
		if(cmd==CMD_SCHEDULE_HOSTGROUP_HOST_DOWNTIME || cmd==CMD_SCHEDULE_HOSTGROUP_SVC_DOWNTIME){
			clean_comment_data(comment_author);
			clean_comment_data(comment_data);
		        }

		break;

	default:
		error=TRUE;
	        }


	/* to be safe, we are going to REQUIRE that the authentication functionality is enabled... */
	if(use_authentication==FALSE){
		if(content_type==WML_CONTENT)
			printf("<p>Error: Authentication is not enabled!</p>\n");
		else{
			printf("<P>\n");
			printf("<DIV CLASS='errorMessage'>Sorry Dave, I can't let you do that...</DIV><br>");
			printf("<DIV CLASS='errorDescription'>");
			printf("It seems that you have chosen to not use the authentication functionality of the CGIs.<br><br>");
			printf("I don't want to be personally responsible for what may happen as a result of allowing unauthorized users to issue commands to Nagios,");
			printf("so you'll have to disable this safeguard if you are really stubborn and want to invite trouble.<br><br>");
			printf("<strong>Read the section on CGI authentication in the HTML documentation to learn how you can enable authentication and why you should want to.</strong>\n");
			printf("</DIV>\n");
			printf("</P>\n");
		        }
	        }

	/* the user is not authorized to issue the given command */
	else if(authorized==FALSE){
		if(content_type==WML_CONTENT)
			printf("<p>Error: You're not authorized to commit that command!</p>\n");
		else{
			printf("<P><DIV CLASS='errorMessage'>Sorry, but you are not authorized to commit the specified command.</DIV></P>\n");
			printf("<P><DIV CLASS='errorDescription'>Read the section of the documentation that deals with authentication and authorization in the CGIs for more information.<BR><BR>\n");
			printf("<A HREF='javascript:window.history.go(-2)'>Return from whence you came</A></DIV></P>\n");
		        }
	        }

	/* some error occurred (data was probably missing) */
	else if(error==TRUE){
		if(content_type==WML_CONTENT)
			printf("<p>An error occurred while processing your command!</p>\n");
		else{
			printf("<P><DIV CLASS='errorMessage'>An error occurred while processing your command.</DIV></P>\n");
			printf("<P><DIV CLASS='errorDescription'>Go back and verify that you entered all required information correctly.<BR>\n");
			printf("<A HREF='javascript:window.history.go(-2)'>Return from whence you came</A></DIV></P>\n");
		        }
	        }

	/* if Nagios isn't checking external commands, don't do anything... */
	else if(check_external_commands==FALSE){
		if(content_type==WML_CONTENT)
			printf("<p>Error: Nagios is not checking external commands!</p>\n");
		else{
			printf("<P><DIV CLASS='errorMessage'>Sorry, but Nagios is currently not checking for external commands, so your command will not be committed!</DIV></P>\n");
			printf("<P><DIV CLASS='errorDescription'>Read the documentation for information on how to enable external commands...<BR><BR>\n");
			printf("<A HREF='javascript:window.history.go(-2)'>Return from whence you came</A></DIV></P>\n");
		        }
	        }
	
	/* everything looks okay, so let's go ahead and commit the command... */
	else{

		/* commit the command */
		result=commit_command(cmd);

		if(result==OK){
			if(content_type==WML_CONTENT)
				printf("<p>Your command was submitted sucessfully...</p>\n");
			else{
				printf("<P><DIV CLASS='infoMessage'>Your command request was successfully submitted to Nagios for processing.<BR><BR>\n");
				printf("Note: It may take a white before the command is actually processed.<BR><BR>\n");
				printf("<A HREF='javascript:window.history.go(-2)'>Done</A></DIV></P>");
			        }
		        }
		else{
			if(content_type==WML_CONTENT)
				printf("<p>An error occurred while committing your command!</p>\n");
			else{
				printf("<P><DIV CLASS='errorMessage'>An error occurred while attempting to commit your command for processing.<BR><BR>\n");
				printf("<A HREF='javascript:window.history.go(-2)'>Return from whence you came</A></DIV></P>\n");
			        }
		        }
	        }

	return;
        }


/* commits a command for processing */
int commit_command(int cmd){
	char command_buffer[MAX_INPUT_BUFFER];
	time_t current_time;
	time_t scheduled_time;
	time_t notification_time;
	int result;

	/* get the current time */
	time(&current_time);

	/* get the scheduled time */
	scheduled_time=current_time+(schedule_delay*60);

	/* get the notification time */
	notification_time=current_time+(notification_delay*60);

	/* decide how to form the command line... */
	switch(cmd){

	case CMD_ADD_HOST_COMMENT:
		snprintf(command_buffer,sizeof(command_buffer)-1,"[%lu] ADD_HOST_COMMENT;%s;%d;%s;%s\n",current_time,host_name,(persistent_comment==TRUE)?1:0,comment_author,comment_data);
		break;
		
	case CMD_ADD_SVC_COMMENT:
		snprintf(command_buffer,sizeof(command_buffer)-1,"[%lu] ADD_SVC_COMMENT;%s;%s;%d;%s;%s\n",current_time,host_name,service_desc,(persistent_comment==TRUE)?1:0,comment_author,comment_data);
		break;

	case CMD_DEL_HOST_COMMENT:
		snprintf(command_buffer,sizeof(command_buffer)-1,"[%lu] DEL_HOST_COMMENT;%d\n",current_time,comment_id);
		break;
		
	case CMD_DEL_SVC_COMMENT:
		snprintf(command_buffer,sizeof(command_buffer)-1,"[%lu] DEL_SVC_COMMENT;%d\n",current_time,comment_id);
		break;
		
	case CMD_DELAY_HOST_NOTIFICATION:
		snprintf(command_buffer,sizeof(command_buffer)-1,"[%lu] DELAY_HOST_NOTIFICATION;%s;%lu\n",current_time,host_name,notification_time);
		break;

	case CMD_DELAY_SVC_NOTIFICATION:
		snprintf(command_buffer,sizeof(command_buffer)-1,"[%lu] DELAY_SVC_NOTIFICATION;%s;%s;%lu\n",current_time,host_name,service_desc,notification_time);
		break;

	case CMD_IMMEDIATE_SVC_CHECK:
	case CMD_DELAY_SVC_CHECK:
		snprintf(command_buffer,sizeof(command_buffer)-1,"[%lu] SCHEDULE_%sSVC_CHECK;%s;%s;%lu\n",current_time,(force_check==TRUE)?"FORCED_":"",host_name,service_desc,(cmd==CMD_IMMEDIATE_SVC_CHECK)?current_time:start_time);
		break;

	case CMD_ENABLE_SVC_CHECK:
	case CMD_DISABLE_SVC_CHECK:
		snprintf(command_buffer,sizeof(command_buffer)-1,"[%lu] %s_SVC_CHECK;%s;%s\n",current_time,(cmd==CMD_ENABLE_SVC_CHECK)?"ENABLE":"DISABLE",host_name,service_desc);
		break;
		
	case CMD_DISABLE_NOTIFICATIONS:
		snprintf(command_buffer,sizeof(command_buffer)-1,"[%lu] DISABLE_NOTIFICATIONS;%lu\n",current_time,scheduled_time);
		break;
		
	case CMD_ENABLE_NOTIFICATIONS:
		snprintf(command_buffer,sizeof(command_buffer)-1,"[%lu] ENABLE_NOTIFICATIONS;%lu\n",current_time,scheduled_time);
		break;
		
	case CMD_SHUTDOWN_PROCESS:
	case CMD_RESTART_PROCESS:
		snprintf(command_buffer,sizeof(command_buffer)-1,"[%lu] %s_PROGRAM;%lu\n",current_time,(cmd==CMD_SHUTDOWN_PROCESS)?"SHUTDOWN":"RESTART",scheduled_time);
		break;

	case CMD_ENABLE_HOST_SVC_CHECKS:
	case CMD_DISABLE_HOST_SVC_CHECKS:
		if(affect_host_and_services==FALSE)
			snprintf(command_buffer,sizeof(command_buffer)-1,"[%lu] %s_HOST_SVC_CHECKS;%s\n",current_time,(cmd==CMD_ENABLE_HOST_SVC_CHECKS)?"ENABLE":"DISABLE",host_name);
		else
			snprintf(command_buffer,sizeof(command_buffer)-1,"[%lu] %s_HOST_SVC_CHECKS;%s\n[%lu] %s_HOST_CHECK;%s\n",current_time,(cmd==CMD_ENABLE_HOST_SVC_CHECKS)?"ENABLE":"DISABLE",host_name,current_time,(cmd==CMD_ENABLE_HOST_SVC_CHECKS)?"ENABLE":"DISABLE",host_name);
		break;
		
	case CMD_DELAY_HOST_SVC_CHECKS:
	case CMD_IMMEDIATE_HOST_SVC_CHECKS:
		snprintf(command_buffer,sizeof(command_buffer)-1,"[%lu] SCHEDULE_%sHOST_SVC_CHECKS;%s;%lu\n",current_time,(force_check==TRUE)?"FORCED_":"",host_name,(cmd==CMD_IMMEDIATE_HOST_SVC_CHECKS)?current_time:scheduled_time);
		break;

	case CMD_DEL_ALL_HOST_COMMENTS:
		snprintf(command_buffer,sizeof(command_buffer)-1,"[%lu] DEL_ALL_HOST_COMMENTS;%s\n",current_time,host_name);
		break;
		
	case CMD_DEL_ALL_SVC_COMMENTS:
		snprintf(command_buffer,sizeof(command_buffer)-1,"[%lu] DEL_ALL_SVC_COMMENTS;%s;%s\n",current_time,host_name,service_desc);
		break;

	case CMD_ENABLE_SVC_NOTIFICATIONS:
	case CMD_DISABLE_SVC_NOTIFICATIONS:
		snprintf(command_buffer,sizeof(command_buffer)-1,"[%lu] %s_SVC_NOTIFICATIONS;%s;%s\n",current_time,(cmd==CMD_ENABLE_SVC_NOTIFICATIONS)?"ENABLE":"DISABLE",host_name,service_desc);
		break;
		
	case CMD_ENABLE_HOST_NOTIFICATIONS:
	case CMD_DISABLE_HOST_NOTIFICATIONS:
		snprintf(command_buffer,sizeof(command_buffer)-1,"[%lu] %s_HOST_NOTIFICATIONS;%s\n",current_time,(cmd==CMD_ENABLE_HOST_NOTIFICATIONS)?"ENABLE":"DISABLE",host_name);
		break;
		
	case CMD_ENABLE_ALL_NOTIFICATIONS_BEYOND_HOST:
	case CMD_DISABLE_ALL_NOTIFICATIONS_BEYOND_HOST:
		snprintf(command_buffer,sizeof(command_buffer)-1,"[%lu] %s_ALL_NOTIFICATIONS_BEYOND_HOST;%s\n",current_time,(cmd==CMD_ENABLE_ALL_NOTIFICATIONS_BEYOND_HOST)?"ENABLE":"DISABLE",host_name);
		break;
		
	case CMD_ENABLE_HOST_SVC_NOTIFICATIONS:
	case CMD_DISABLE_HOST_SVC_NOTIFICATIONS:
		if(affect_host_and_services==FALSE)
			snprintf(command_buffer,sizeof(command_buffer)-1,"[%lu] %s_HOST_SVC_NOTIFICATIONS;%s\n",current_time,(cmd==CMD_ENABLE_HOST_SVC_NOTIFICATIONS)?"ENABLE":"DISABLE",host_name);
		else
			snprintf(command_buffer,sizeof(command_buffer)-1,"[%lu] %s_HOST_SVC_NOTIFICATIONS;%s\n[%lu] %s_HOST_NOTIFICATIONS;%s\n",current_time,(cmd==CMD_ENABLE_HOST_SVC_NOTIFICATIONS)?"ENABLE":"DISABLE",host_name,current_time,(cmd==CMD_ENABLE_HOST_SVC_NOTIFICATIONS)?"ENABLE":"DISABLE",host_name);
		break;
		
	case CMD_ACKNOWLEDGE_HOST_PROBLEM:
		snprintf(command_buffer,sizeof(command_buffer)-1,"[%lu] ADD_HOST_COMMENT;%s;%d;%s;ACKNOWLEDGEMENT: %s\n[%lu] ACKNOWLEDGE_HOST_PROBLEM;%s;%d;%s\n",current_time,host_name,(persistent_comment==TRUE)?1:0,comment_author,comment_data,current_time,host_name,(send_notification==TRUE)?1:0,comment_data);
		break;
		
	case CMD_ACKNOWLEDGE_SVC_PROBLEM:
		snprintf(command_buffer,sizeof(command_buffer)-1,"[%lu] ADD_SVC_COMMENT;%s;%s;%d;%s;ACKNOWLEDGEMENT: %s\n[%lu] ACKNOWLEDGE_SVC_PROBLEM;%s;%s;%d;%s\n",current_time,host_name,service_desc,(persistent_comment==TRUE)?1:0,comment_author,comment_data,current_time,host_name,service_desc,(send_notification==TRUE)?1:0,comment_data);
		break;

	case CMD_START_EXECUTING_SVC_CHECKS:
	case CMD_STOP_EXECUTING_SVC_CHECKS:
		snprintf(command_buffer,sizeof(command_buffer)-1,"[%lu] %s_EXECUTING_SVC_CHECKS;\n",current_time,(cmd==CMD_START_EXECUTING_SVC_CHECKS)?"START":"STOP");
		break;

	case CMD_START_ACCEPTING_PASSIVE_SVC_CHECKS:
	case CMD_STOP_ACCEPTING_PASSIVE_SVC_CHECKS:
		snprintf(command_buffer,sizeof(command_buffer)-1,"[%lu] %s_ACCEPTING_PASSIVE_SVC_CHECKS;\n",current_time,(cmd==CMD_START_ACCEPTING_PASSIVE_SVC_CHECKS)?"START":"STOP");
		break;

	case CMD_ENABLE_PASSIVE_SVC_CHECKS:
	case CMD_DISABLE_PASSIVE_SVC_CHECKS:
		snprintf(command_buffer,sizeof(command_buffer)-1,"[%lu] %s_PASSIVE_SVC_CHECKS;%s;%s\n",current_time,(cmd==CMD_ENABLE_PASSIVE_SVC_CHECKS)?"ENABLE":"DISABLE",host_name,service_desc);
		break;
		
	case CMD_ENABLE_EVENT_HANDLERS:
	case CMD_DISABLE_EVENT_HANDLERS:
		snprintf(command_buffer,sizeof(command_buffer)-1,"[%lu] %s_EVENT_HANDLERS;\n",current_time,(cmd==CMD_ENABLE_EVENT_HANDLERS)?"ENABLE":"DISABLE");
		break;

	case CMD_ENABLE_SVC_EVENT_HANDLER:
	case CMD_DISABLE_SVC_EVENT_HANDLER:
		snprintf(command_buffer,sizeof(command_buffer)-1,"[%lu] %s_SVC_EVENT_HANDLER;%s;%s\n",current_time,(cmd==CMD_ENABLE_SVC_EVENT_HANDLER)?"ENABLE":"DISABLE",host_name,service_desc);
		break;
		
	case CMD_ENABLE_HOST_EVENT_HANDLER:
	case CMD_DISABLE_HOST_EVENT_HANDLER:
		snprintf(command_buffer,sizeof(command_buffer)-1,"[%lu] %s_HOST_EVENT_HANDLER;%s\n",current_time,(cmd==CMD_ENABLE_HOST_EVENT_HANDLER)?"ENABLE":"DISABLE",host_name);
		break;
		
	case CMD_ENABLE_HOST_CHECK:
	case CMD_DISABLE_HOST_CHECK:
		snprintf(command_buffer,sizeof(command_buffer)-1,"[%lu] %s_HOST_CHECK;%s\n",current_time,(cmd==CMD_ENABLE_HOST_CHECK)?"ENABLE":"DISABLE",host_name);
		break;
		
	case CMD_START_OBSESSING_OVER_SVC_CHECKS:
	case CMD_STOP_OBSESSING_OVER_SVC_CHECKS:
		snprintf(command_buffer,sizeof(command_buffer)-1,"[%lu] %s_OBSESSING_OVER_SVC_CHECKS;\n",current_time,(cmd==CMD_START_OBSESSING_OVER_SVC_CHECKS)?"START":"STOP");
		break;
		
	case CMD_REMOVE_HOST_ACKNOWLEDGEMENT:
		snprintf(command_buffer,sizeof(command_buffer)-1,"[%lu] REMOVE_HOST_ACKNOWLEDGEMENT;%s\n",current_time,host_name);
		break;
		
	case CMD_REMOVE_SVC_ACKNOWLEDGEMENT:
		snprintf(command_buffer,sizeof(command_buffer)-1,"[%lu] REMOVE_SVC_ACKNOWLEDGEMENT;%s;%s\n",current_time,host_name,service_desc);
		break;
		
	case CMD_PROCESS_SERVICE_CHECK_RESULT:
		snprintf(command_buffer,sizeof(command_buffer)-1,"[%lu] PROCESS_SERVICE_CHECK_RESULT;%s;%s;%d;%s\n",current_time,host_name,service_desc,plugin_state,plugin_output);
		break;
		
	case CMD_SCHEDULE_HOST_DOWNTIME:
		snprintf(command_buffer,sizeof(command_buffer)-1,"[%lu] SCHEDULE_HOST_DOWNTIME;%s;%lu;%lu;%d;%lu;%s;%s\n",current_time,host_name,start_time,end_time,(fixed==TRUE)?1:0,duration,comment_author,comment_data);
		break;
		
	case CMD_SCHEDULE_SVC_DOWNTIME:
		snprintf(command_buffer,sizeof(command_buffer)-1,"[%lu] SCHEDULE_SVC_DOWNTIME;%s;%s;%lu;%lu;%d;%lu;%s;%s\n",current_time,host_name,service_desc,start_time,end_time,(fixed==TRUE)?1:0,duration,comment_author,comment_data);
		break;
		
	case CMD_ENABLE_HOST_FLAP_DETECTION:
	case CMD_DISABLE_HOST_FLAP_DETECTION:
		snprintf(command_buffer,sizeof(command_buffer)-1,"[%lu] %s_HOST_FLAP_DETECTION;%s\n",current_time,(cmd==CMD_ENABLE_HOST_FLAP_DETECTION)?"ENABLE":"DISABLE",host_name);
		break;
		
	case CMD_ENABLE_SVC_FLAP_DETECTION:
	case CMD_DISABLE_SVC_FLAP_DETECTION:
		snprintf(command_buffer,sizeof(command_buffer)-1,"[%lu] %s_SVC_FLAP_DETECTION;%s;%s\n",current_time,(cmd==CMD_ENABLE_SVC_FLAP_DETECTION)?"ENABLE":"DISABLE",host_name,service_desc);
		break;
		
	case CMD_ENABLE_FLAP_DETECTION:
	case CMD_DISABLE_FLAP_DETECTION:
		snprintf(command_buffer,sizeof(command_buffer)-1,"[%lu] %s_FLAP_DETECTION\n",current_time,(cmd==CMD_ENABLE_FLAP_DETECTION)?"ENABLE":"DISABLE");
		break;
		
	case CMD_ENABLE_HOSTGROUP_SVC_NOTIFICATIONS:
	case CMD_DISABLE_HOSTGROUP_SVC_NOTIFICATIONS:
	case CMD_ENABLE_HOSTGROUP_HOST_NOTIFICATIONS:
	case CMD_DISABLE_HOSTGROUP_HOST_NOTIFICATIONS:
	case CMD_ENABLE_HOSTGROUP_SVC_CHECKS:
	case CMD_DISABLE_HOSTGROUP_SVC_CHECKS:
	case CMD_SCHEDULE_HOSTGROUP_HOST_DOWNTIME:
	case CMD_SCHEDULE_HOSTGROUP_SVC_DOWNTIME:
		result=commit_hostgroup_command(cmd);
		return result;
		break;

	case CMD_DEL_HOST_DOWNTIME:
	case CMD_DEL_SVC_DOWNTIME:
		snprintf(command_buffer,sizeof(command_buffer)-1,"[%lu] DEL_%s_DOWNTIME;%d\n",current_time,(cmd==CMD_DEL_HOST_DOWNTIME)?"HOST":"SVC",downtime_id);
		break;

	case CMD_ENABLE_FAILURE_PREDICTION:
	case CMD_DISABLE_FAILURE_PREDICTION:
		snprintf(command_buffer,sizeof(command_buffer)-1,"[%lu] %s_FAILURE_PREDICTION\n",current_time,(cmd==CMD_ENABLE_FAILURE_PREDICTION)?"ENABLE":"DISABLE");
		break;
		
	case CMD_ENABLE_PERFORMANCE_DATA:
	case CMD_DISABLE_PERFORMANCE_DATA:
		snprintf(command_buffer,sizeof(command_buffer)-1,"[%lu] %s_PERFORMANCE_DATA\n",current_time,(cmd==CMD_ENABLE_PERFORMANCE_DATA)?"ENABLE":"DISABLE");
		break;
		
	default:
		return ERROR;
		break;
	        }

	/* make sure command buffer is terminated */
	command_buffer[sizeof(command_buffer)-1]='\x0';

	/* write the command to the command file */
	result=write_command_to_file(command_buffer);

	return result;
        }



/* commits one or more hostgroup commands for processing */
int commit_hostgroup_command(int cmd){
	hostgroup *temp_hostgroup=NULL;
	host *temp_host=NULL;
	char command_buffer[MAX_INPUT_BUFFER];
	time_t current_time;
	time_t scheduled_time;
	int result;

	/* get the current time */
	time(&current_time);

	/* get the scheduled time */
	scheduled_time=current_time+(schedule_delay*60);

	/* find the hostgroup */
	temp_hostgroup=find_hostgroup(hostgroup_name,NULL);
	if(temp_hostgroup==NULL)
		return ERROR;

	/* find all hosts that belong to this hostgroup... */
	for(temp_host=host_list;temp_host!=NULL;temp_host=temp_host->next){

		/* skip this host if it's not part of the hostgroup */
		if(is_host_member_of_hostgroup(temp_hostgroup,temp_host)==FALSE)
			continue;

		/* is the user authorized to issue command for this host? */
		if(is_authorized_for_host_commands(temp_host,&current_authdata)==FALSE)
			continue;

		/* blank the command line */
		strcpy(command_buffer,"");

		/* decide how to form the command line... */
		switch(cmd){
		
		case CMD_ENABLE_HOSTGROUP_SVC_NOTIFICATIONS:
		case CMD_DISABLE_HOSTGROUP_SVC_NOTIFICATIONS:
			if(affect_host_and_services==FALSE)
				snprintf(command_buffer,sizeof(command_buffer)-1,"[%lu] %s_HOST_SVC_NOTIFICATIONS;%s\n",current_time,(cmd==CMD_ENABLE_HOSTGROUP_SVC_NOTIFICATIONS)?"ENABLE":"DISABLE",temp_host->name);
			else
				snprintf(command_buffer,sizeof(command_buffer)-1,"[%lu] %s_HOST_SVC_NOTIFICATIONS;%s\n[%lu] %s_HOST_NOTIFICATIONS;%s\n",current_time,(cmd==CMD_ENABLE_HOSTGROUP_SVC_NOTIFICATIONS)?"ENABLE":"DISABLE",temp_host->name,current_time,(cmd==CMD_ENABLE_HOSTGROUP_SVC_NOTIFICATIONS)?"ENABLE":"DISABLE",temp_host->name);
			break;

		case CMD_ENABLE_HOSTGROUP_HOST_NOTIFICATIONS:
		case CMD_DISABLE_HOSTGROUP_HOST_NOTIFICATIONS:
			snprintf(command_buffer,sizeof(command_buffer)-1,"[%lu] %s_HOST_NOTIFICATIONS;%s\n",current_time,(cmd==CMD_ENABLE_HOSTGROUP_HOST_NOTIFICATIONS)?"ENABLE":"DISABLE",temp_host->name);
			break;

		case CMD_ENABLE_HOSTGROUP_SVC_CHECKS:
		case CMD_DISABLE_HOSTGROUP_SVC_CHECKS:
			if(affect_host_and_services==FALSE)
				snprintf(command_buffer,sizeof(command_buffer)-1,"[%lu] %s_HOST_SVC_CHECKS;%s\n",current_time,(cmd==CMD_ENABLE_HOSTGROUP_SVC_CHECKS)?"ENABLE":"DISABLE",temp_host->name);
			else
				snprintf(command_buffer,sizeof(command_buffer)-1,"[%lu] %s_HOST_SVC_CHECKS;%s\n[%lu] %s_HOST_CHECK;%s\n",current_time,(cmd==CMD_ENABLE_HOSTGROUP_SVC_CHECKS)?"ENABLE":"DISABLE",temp_host->name,current_time,(cmd==CMD_ENABLE_HOSTGROUP_SVC_CHECKS)?"ENABLE":"DISABLE",temp_host->name);
			break;

		case CMD_SCHEDULE_HOSTGROUP_HOST_DOWNTIME:
			snprintf(command_buffer,sizeof(command_buffer)-1,"[%lu] SCHEDULE_HOST_DOWNTIME;%s;%lu;%lu;%d;%lu;%s;%s\n",current_time,temp_host->name,start_time,end_time,(fixed==TRUE)?1:0,duration,comment_author,comment_data);
			break;

		case CMD_SCHEDULE_HOSTGROUP_SVC_DOWNTIME:
			if(affect_host_and_services==FALSE)
				snprintf(command_buffer,sizeof(command_buffer)-1,"[%lu] SCHEDULE_HOST_SVC_DOWNTIME;%s;%lu;%lu;%d;%lu;%s;%s\n",current_time,temp_host->name,start_time,end_time,(fixed==TRUE)?1:0,duration,comment_author,comment_data);
			else{
				snprintf(command_buffer,sizeof(command_buffer)-1,"[%lu] SCHEDULE_HOST_DOWNTIME;%s;%lu;%lu;%d;%lu;%s;%s\n[%lu] SCHEDULE_HOST_SVC_DOWNTIME;%s;%lu;%lu;%d;%lu;%s;%s\n",current_time,temp_host->name,start_time,end_time,(fixed==TRUE)?1:0,duration,comment_author,comment_data,current_time,temp_host->name,start_time,end_time,(fixed==TRUE)?1:0,duration,comment_author,comment_data);
			        }
			break;

		default:
			return ERROR;
			break;
	                }

		/* make sure command buffer is terminated */
		command_buffer[sizeof(command_buffer)-1]='\x0';

		/* write the command to the command file */
		result=write_command_to_file(command_buffer);

		/* bail out if we encountered an error */
		if(result!=OK)
			return result;
	        }

	return OK;
        }



/* write a command entry to the command file */
int write_command_to_file(char *cmd){
	FILE *fp;
	struct stat statbuf;

	/* bail out if the external command file doesn't exist */
	if(stat(command_file,&statbuf)){

		if(content_type==WML_CONTENT)
			printf("<p>Error: Could not stat() external command file!</p>\n");
		else{
			printf("<P><DIV CLASS='errorMessage'>Error: Could not stat() command file '%s'!</DIV></P>\n",command_file);
			printf("<P><DIV CLASS='errorDescription'>");
			printf("The external command file may be missing, Nagios may not be running, and/or Nagios may not be checking external commands.\n");
			printf("</DIV></P>\n");
			}

		return ERROR;
	        }

 	/* open the command for writing (since this is a pipe, it will really be appended) */
	fp=fopen(command_file,"w+");
	if(fp==NULL){

		if(content_type==WML_CONTENT)
			printf("<p>Error: Could not open command file for update!</p>\n");
		else{
			printf("<P><DIV CLASS='errorMessage'>Error: Could not open command file '%s' for update!</DIV></P>\n",command_file);
			printf("<P><DIV CLASS='errorDescription'>");
			printf("The permissions on the external command file and/or directory may be incorrect.  Read the FAQs on how to setup proper permissions.\n");
			printf("</DIV></P>\n");
			}

		return ERROR;
	        }

	/* write the command to file */
	fputs(cmd,fp);

	/* flush buffer */
	fflush(fp);

	fclose(fp);

	return OK;
        }


/* strips out semicolons from comment data */
void clean_comment_data(char *buffer){
	int x;
	int y;

	y=(int)strlen(buffer);
	
	for(x=0;x<y;x++){
		if(buffer[x]==';')
			buffer[x]=' ';
	        }

	return;
        }


/* display information about a command */
void show_command_help(cmd){

	printf("<DIV ALIGN=CENTER CLASS='descriptionTitle'>Command Description</DIV>\n");
	printf("<TABLE BORDER=1 CELLSPACING=0 CELLPADDING=0 CLASS='commandDescription'>\n");
	printf("<TR><TD CLASS='commandDescription'>\n");

	/* decide what information to print out... */
	switch(cmd){

	case CMD_ADD_HOST_COMMENT:
		printf("This command is used to add a comment for the specified host.  If you work with other administrators, you may find it useful to share information about a host\n");
		printf("that is having problems if more than one of you may be working on it.  If you do not check the 'persistent' option, the comment will be automatically be deleted\n");
		printf("the next time Nagios is restarted.\n");
		break;
		
	case CMD_ADD_SVC_COMMENT:
		printf("This command is used to add a comment for the specified service.  If you work with other administrators, you may find it useful to share information about a host\n");
		printf("or service that is having problems if more than one of you may be working on it.  If you do not check the 'persistent' option, the comment will automatically be\n");
		printf("deleted the next time Nagios is restarted.\n");
		break;

	case CMD_DEL_HOST_COMMENT:
		printf("This command is used to delete a specific host comment.\n");
		break;
		
	case CMD_DEL_SVC_COMMENT:
		printf("This command is used to delete a specific service comment.\n");
		break;
		
	case CMD_DELAY_HOST_NOTIFICATION:
		printf("This command is used to delay the next problem notification that is sent out for the specified host.  The notification delay will be disregarded if\n");
		printf("the host changes state before the next notification is scheduled to be sent out.  This command has no effect if the host is currently UP.\n");
		break;

	case CMD_DELAY_SVC_NOTIFICATION:
		printf("This command is used to delay the next problem notification that is sent out for the specified service.  The notification delay will be disregarded if\n");
		printf("the service changes state before the next notification is scheduled to be sent out.  This command has no effect if the service is currently in an OK state.\n");
		break;

	case CMD_DELAY_SVC_CHECK:
		printf("This command is used to re-schedule the next check of a particular service.  Nagios will re-queue the service to be checked at the time you specify.\n");
		printf("If you select the <i>force check</i> option, Nagios will force a check of the service regardless of both what time the scheduled check occurs and whether or not checks are enabled for the service.\n");
		break;

	case CMD_IMMEDIATE_SVC_CHECK:
		printf("This command will schedule an immediate check of the specified service.  Note that the check is <i>scheduled</i> immediately, not necessary executed immediately.  If Nagios\n");
		printf("has fallen behind in its scheduling queue, it will check services that were queued prior to this one.\n");
		printf("If you select the <i>force check</i> option, Nagios will force a check of the service regardless of both what time the scheduled check occurs and whether or not checks are enabled for the service.\n");
		break;

	case CMD_ENABLE_SVC_CHECK:
		printf("This command is used to re-enable a service check that has been disabled.  Once a service check is enabled, Nagios will begin to monitor the service as usual.  If the service\n");
		printf("has recovered from a problem that was detected before the check was disabled, contacts might be notified of the recovery.\n");
		break;
		
	case CMD_DISABLE_SVC_CHECK:
		printf("This command is used to disable a service check.  When a service is disabled Nagios will not monitor the service.  Doing this will prevent any notifications being sent out for\n");
		printf("the specified service while it is disabled.  In order to have Nagios check the service in the future you will have to re-enable the service.\n");
		printf("Note that disabling service checks may not necessarily prevent notifications from being sent out about the host which those services are associated with.\n");
		break;
		
	case CMD_DISABLE_NOTIFICATIONS:
		printf("This command is used to disable host and service notifications on a program-wide basis.\n");
		break;
		
	case CMD_ENABLE_NOTIFICATIONS:
		printf("This command is used to enable host and service notifications on a program-wide basis.\n");
		break;
		
	case CMD_SHUTDOWN_PROCESS:
		printf("This command is used to shutdown the Nagios process. Note: Once the Nagios has been shutdown, it cannot be restarted via the web interface!\n");
		break;

	case CMD_RESTART_PROCESS:
		printf("This command is used to restart the Nagios process.   Executing a restart command is equivalent to sending the process a HUP signal.\n");
		printf("All information will be flushed from memory, the configuration files will be re-read, and Nagios will start monitoring with the new configuration information.\n");
		break;

	case CMD_ENABLE_HOST_SVC_CHECKS:
		printf("This command is used to enable all service checks associated with the specified host.  This <i>does not</i> enable checks of the host unless you check the 'Enable for host too' option.\n");
		break;
		
	case CMD_DISABLE_HOST_SVC_CHECKS:
		printf("This command is used to disable all service checks associated with the specified host.  When a service is disabled Nagios will not monitor the service.  Doing this will prevent any notifications being sent out for\n");
		printf("the specified service while it is disabled.  In order to have Nagios check the service in the future you will have to re-enable the service.\n");
		printf("Note that disabling service checks may not necessarily prevent notifications from being sent out about the host which those services are associated with.  This <i>does not</i> disable checks of the host unless you check the 'Disable for host too' option.\n");
		break;
		
	case CMD_DELAY_HOST_SVC_CHECKS:
		printf("This command is used to delay the next scheduled check of all services on the specified host.  Nagios will re-queue the services and will not not check them again until the number\n");
		printf("of minutes you specified with the <b><i>check delay</i></b> option has elapsed from the time the command is committed.  Specifying a value of 0 for the delay value\n");
		printf("is equivalent to scheduling an immediate check of all the services.\n");
		printf("If you select the <i>force check</i> option, Nagios will force a check of all services on the host regardless of both what time the scheduled checks occur and whether or not checks are enabled for those services.\n");
		break;

	case CMD_IMMEDIATE_HOST_SVC_CHECKS:
		printf("This command will schedule an immediate check of all service on the specified host.  Note that the checks are <i>scheduled</i> immediately, not necessary executed immediately.  If Nagios\n");
		printf("has fallen behind in its scheduling queue, it will check services that were queued prior to these services.\n");
		printf("If you select the <i>force check</i> option, Nagios will force a check of all services on the host regardless of both what time the scheduled checks occur and whether or not checks are enabled for those services.\n");
		break;

	case CMD_DEL_ALL_HOST_COMMENTS:
		printf("This command is used to delete all comments associated with the specified host.\n");
		break;
		
	case CMD_DEL_ALL_SVC_COMMENTS:
		printf("This command is used to delete all comments associated with the specified service.\n");
		break;

	case CMD_ENABLE_SVC_NOTIFICATIONS:
		printf("This command is used to enable notifications for the specified service.  Notifications will only be sent out for the\n");
		printf("service state types you defined in your service definition.\n");
		break;

	case CMD_DISABLE_SVC_NOTIFICATIONS:
		printf("This command is used to prevent notifications from being sent out for the specified service.  You will have to re-enable notifications\n");
		printf("for this service before any alerts can be sent out in the future.\n");
		break;

	case CMD_ENABLE_HOST_NOTIFICATIONS:
		printf("This command is used to enable notifications for the specified host.  Notifications will only be sent out for the\n");
		printf("host state types you defined in your host definition.  Note that this command <i>does not</i> enable notifications\n");
		printf("for services associated with this host.\n");
		break;

	case CMD_DISABLE_HOST_NOTIFICATIONS:
		printf("This command is used to prevent notifications from being sent out for the specified host.  You will have to re-enable notifications for this host\n");
		printf("before any alerts can be sent out in the future.  Note that this command <i>does not</i> disable notifications for services associated with this host.\n");
		break;

	case CMD_ENABLE_ALL_NOTIFICATIONS_BEYOND_HOST:
		printf("This command is used to enable notifications for all hosts and services that lie \"beyond\" the specified host\n");
		printf("(from the view of Nagios).\n");
		break;

	case CMD_DISABLE_ALL_NOTIFICATIONS_BEYOND_HOST:
		printf("This command is used to temporarily prevent notifications from being sent out for all hosts and services that lie\n");
		printf("\"beyone\" the specified host (from the view of Nagios).\n");
		break;
		
	case CMD_ENABLE_HOST_SVC_NOTIFICATIONS:
		printf("This command is used to enable notifications for all services on the specified host.  Notifications will only be sent out for the\n");
		printf("service state types you defined in your service definition.  This <i>does not</i> enable notifications for the host unless you check the 'Enable for host too' option.\n");
		break;

	case CMD_DISABLE_HOST_SVC_NOTIFICATIONS:
		printf("This command is used to prevent notifications from being sent out for all services on the specified host.  You will have to re-enable notifications for\n");
		printf("all services associated with this host before any alerts can be sent out in the future.  This <i>does not</i> prevent notifications from being sent out about the host unless you check the 'Disable for host too' option.\n");
		break;

	case CMD_ACKNOWLEDGE_HOST_PROBLEM:
		printf("This command is used to acknowledge a host problem.  When a host problem is acknowledged, future notifications about problems are temporarily disabled until the host changes state (i.e. recovers).\n");
		printf("Contacts for this host will receive a notification about the acknowledgement, so they are aware that someone is working on the problem.  Additionally, a comment will also be added to the host.\n");
		printf("Make sure to enter your name and fill in a brief description of what you are doing in the comment field.  If you would like the host comment to be retained between restarts of Nagios, check\n");
		printf("the 'Persistent' checkbox.  If you do not want an acknowledgement notification sent out to the appropriate contacts, uncheck the 'Send Notification' checkbox.\n");
		break;

	case CMD_ACKNOWLEDGE_SVC_PROBLEM:
		printf("This command is used to acknowledge a service problem.  When a service problem is acknowledged, future notifications about problems are temporarily disabled until the service changes state (i.e. recovers).\n");
		printf("Contacts for this service will receive a notification about the acknowledgement, so they are aware that someone is working on the problem.  Additionally, a comment will also be added to the service.\n");
		printf("Make sure to enter your name and fill in a brief description of what you are doing in the comment field.  If you would like the service comment to be retained between restarts of Nagios, check\n");
		printf("the 'Persistent' checkbox.  If you do not want an acknowledgement notification sent out to the appropriate contacts, uncheck the 'Send Notification' checkbox.\n");
		break;

	case CMD_START_EXECUTING_SVC_CHECKS:
		printf("This command is used to resume execution of service checks on a program-wide basis.  Individual services which are disabled will still not be checked.\n");
		break;

	case CMD_STOP_EXECUTING_SVC_CHECKS:
		printf("This command is used to temporarily stop Nagios from executing any service checks.  This will have the side effect of preventing any notifications from being sent out (for any and all services and hosts).\n");
		printf("Service checks will not be executed again until you issue a command to resume service check execution.\n");
		break;

	case CMD_START_ACCEPTING_PASSIVE_SVC_CHECKS:
		printf("This command is used to make Nagios start accepting passive service check results that it finds in the external command file\n");
		break;

	case CMD_STOP_ACCEPTING_PASSIVE_SVC_CHECKS:
		printf("This command is use to make Nagios stop accepting passive service check results that it finds in the external command file.  All passive check results that are found will be ignored.\n");
		break;

	case CMD_ENABLE_PASSIVE_SVC_CHECKS:
		printf("This command is used to allow Nagios to accept passive service check results that it finds in the external command file for this particular service.\n");
		break;

	case CMD_DISABLE_PASSIVE_SVC_CHECKS:
		printf("This command is used to make Nagios stop accepting passive service check results that it finds in the external command file for this particular service.  All passive check results that are found for this service will be ignored.\n");
		break;

	case CMD_ENABLE_EVENT_HANDLERS:
		printf("This command is used to allow Nagios to run host and service event handlers.\n");
		break;

	case CMD_DISABLE_EVENT_HANDLERS:
		printf("This command is used to temporarily prevent Nagios from running any host or service event handlers.\n");
		break;

	case CMD_ENABLE_SVC_EVENT_HANDLER:
		printf("This command is used to allow Nagios to run the service event handler for a particular service when necessary (if one is defined).\n");
		break;

	case CMD_DISABLE_SVC_EVENT_HANDLER:
		printf("This command is used to temporarily prevent Nagios from running the service event handler for a particular service.\n");
		break;

	case CMD_ENABLE_HOST_EVENT_HANDLER:
		printf("This command is used to allow Nagios to run the host event handler for a particular service when necessary (if one is defined).\n");
		break;

	case CMD_DISABLE_HOST_EVENT_HANDLER:
		printf("This command is used to temporarily prevent Nagios from running the host event handler for a particular host.\n");
		break;

	case CMD_ENABLE_HOST_CHECK:
		printf("This command is used to enable checks of this host.\n");
		break;

	case CMD_DISABLE_HOST_CHECK:
		printf("This command is used to temporarily prevent Nagios from checking the status of a particular host.  If Nagios needs to check the status of this host, it will assume that it is in the same state that it was in before checks were disabled.\n");
		break;

	case CMD_START_OBSESSING_OVER_SVC_CHECKS:
		printf("This command is used to have Nagios start obsessing over service checks.  Read the documentation on distributed monitoring for more information on this.\n");
		break;

	case CMD_STOP_OBSESSING_OVER_SVC_CHECKS:
		printf("This command is used to have Nagios stop obsessing over service checks.  Read the documentation on distributed monitoring for more information on this.\n");
		break;

	case CMD_REMOVE_HOST_ACKNOWLEDGEMENT:
		printf("This command is used to remove an acknowledgement for a particular host problem.  Once the acknowledgement is removed, notifications may start being\n");
		printf("sent out about the host problem.  Note: Removing the acknowledgement does <i>not</i> remove the host comment that was originally associated\n");
		printf("with the acknowledgement.  You'll have to remove that as well if that's what you want.\n");
		break;

	case CMD_REMOVE_SVC_ACKNOWLEDGEMENT:
		printf("This command is used to remove an acknowledgement for a particular service problem.  Once the acknowledgement is removed, notifications may start being\n");
		printf("sent out about the service problem.  Note: Removing the acknowledgement does <i>not</i> remove the service comment that was originally associated\n");
		printf("with the acknowledgement.  You'll have to remove that as well if that's what you want.\n");
		break;

	case CMD_PROCESS_SERVICE_CHECK_RESULT:
		printf("This command is used to submit a passive check result for a particular service.  It is particularly useful for resetting security-related services to OK states once they have been dealt with.\n");
		break;

	case CMD_SCHEDULE_HOST_DOWNTIME:
		printf("This command is used to schedule downtime for a particular host.  During the specified downtime, Nagios will not send notifications out about the host.\n");
		printf("When the scheduled downtime expires, Nagios will send out notifications for this host as it normally would.  Scheduled downtimes are preserved\n");
		printf("across program shutdowns and restarts.  Both the start and end times should be specified in the following format:  <b>mm/dd/yyyy hh:mm:ss</b>.\n");
		printf("If you select the <i>fixed</i> option, the downtime will be in effect between the start and end times you specify.  If you do not select the <i>fixed</i>\n");
		printf("option, Nagios will treat this as \"flexible\" downtime.  Flexible downtime starts when the host goes down or becomes unreachable (sometime between the\n");
		printf("start and end times you specified) and lasts as long as the duration of time you enter.  The duration fields do not apply for fixed dowtime.\n");
		break;

	case CMD_SCHEDULE_SVC_DOWNTIME:
		printf("This command is used to schedule downtime for a particular service.  During the specified downtime, Nagios will not send notifications out about the service.\n");
		printf("When the scheduled downtime expires, Nagios will send out notifications for this service as it normally would.  Scheduled downtimes are preserved\n");
		printf("across program shutdowns and restarts.  Both the start and end times should be specified in the following format:  <b>mm/dd/yyyy hh:mm:ss</b>.\n");
		printf("option, Nagios will treat this as \"flexible\" downtime.  Flexible downtime starts when the service enters a non-OK state (sometime between the\n");
		printf("start and end times you specified) and lasts as long as the duration of time you enter.  The duration fields do not apply for fixed dowtime.\n");
		break;

	case CMD_ENABLE_HOST_FLAP_DETECTION:
		printf("This command is used to enable flap detection for a specific host.  If flap detection is disabled on a program-wide basis, this will have no effect,\n");
		break;

	case CMD_DISABLE_HOST_FLAP_DETECTION:
		printf("This command is used to disable flap detection for a specific host.\n");
		break;

	case CMD_ENABLE_SVC_FLAP_DETECTION:
		printf("This command is used to enable flap detection for a specific service.  If flap detection is disabled on a program-wide basis, this will have no effect,\n");
		break;

	case CMD_DISABLE_SVC_FLAP_DETECTION:
		printf("This command is used to disable flap detection for a specific service.\n");
		break;

	case CMD_ENABLE_FLAP_DETECTION:
		printf("This command is used to enable flap detection for hosts and services on a program-wide basis.  Individual hosts and services may have flap detection disabled.\n");
		break;

	case CMD_DISABLE_FLAP_DETECTION:
		printf("This command is used to disable flap detection for hosts and services on a program-wide basis.\n");
		break;

	case CMD_ENABLE_HOSTGROUP_SVC_NOTIFICATIONS:
		printf("This command is used to enable notifications for all services in the specified hostgroup.  Notifications will only be sent out for the\n");
		printf("service state types you defined in your service definitions.  This <i>does not</i> enable notifications for the hosts in this hostgroup unless you check the 'Enable for hosts too' option.\n");
		break;

	case CMD_DISABLE_HOSTGROUP_SVC_NOTIFICATIONS:
		printf("This command is used to prevent notifications from being sent out for all services in the specified hostgroup.  You will have to re-enable notifications for\n");
		printf("all services in this hostgroup before any alerts can be sent out in the future.  This <i>does not</i> prevent notifications from being sent out about the hosts in this hostgroup unless you check the 'Disable for hosts too' option.\n");
		break;

	case CMD_ENABLE_HOSTGROUP_HOST_NOTIFICATIONS:
		printf("This command is used to enable notifications for all hosts in the specified hostgroup.  Notifications will only be sent out for the\n");
		printf("host state types you defined in your host definitions.\n");
		break;

	case CMD_DISABLE_HOSTGROUP_HOST_NOTIFICATIONS:
		printf("This command is used to prevent notifications from being sent out for all hosts in the specified hostgroup.  You will have to re-enable notifications for\n");
		printf("all hosts in this hostgroup before any alerts can be sent out in the future.\n");
		break;

	case CMD_ENABLE_HOSTGROUP_SVC_CHECKS:
		printf("This command is used to enable all service checks in the specified hostgroup.  This <i>does not</i> enable checks of the hosts in the hostgroup unless you check the 'Enable for hosts too' option.\n");
		break;
		
	case CMD_DISABLE_HOSTGROUP_SVC_CHECKS:
		printf("This command is used to disable all service checks in the specified hostgroup.  When a service is disabled Nagios will not monitor the service.  Doing this will prevent any notifications being sent out for\n");
		printf("the specified service while it is disabled.  In order to have Nagios check the services in the future you will have to re-enable the services.\n");
		printf("Note that disabling service checks may not necessarily prevent notifications from being sent out about the host which those services are associated with.  This <i>does not</i> disable checks of the hosts in the hostgroup unless you check the 'Disable for hosts too' option.\n");
		break;

	case CMD_DEL_HOST_DOWNTIME:
		printf("This command is used to cancel active or pending scheduled downtime for the specified host.\n");
		break;

	case CMD_DEL_SVC_DOWNTIME:
		printf("This command is used to cancel active or pending scheduled downtime for the specified service.\n");
		break;

	case CMD_ENABLE_FAILURE_PREDICTION:
		printf("This command is used to enable failure prediction for hosts and services on a program-wide basis.  Individual hosts and services may have failure prediction disabled.\n");
		break;

	case CMD_DISABLE_FAILURE_PREDICTION:
		printf("This command is used to disable failure prediction for hosts and services on a program-wide basis.\n");
		break;

	case CMD_ENABLE_PERFORMANCE_DATA:
		printf("This command is used to enable the processing of performance data for hosts and services on a program-wide basis.  Individual hosts and services may have performance data processing disabled.\n");
		break;

	case CMD_DISABLE_PERFORMANCE_DATA:
		printf("This command is used to disable the processing of performance data for hosts and services on a program-wide basis.\n");
		break;

	case CMD_SCHEDULE_HOSTGROUP_HOST_DOWNTIME:
		printf("This command is used to schedule downtime for all hosts in a particular hostgroup.  During the specified downtime, Nagios will not send notifications out about the hosts.\n");
		printf("When the scheduled downtime expires, Nagios will send out notifications for the hosts as it normally would.  Scheduled downtimes are preserved\n");
		printf("across program shutdowns and restarts.  Both the start and end times should be specified in the following format:  <b>mm/dd/yyyy hh:mm:ss</b>.\n");
		printf("If you select the <i>fixed</i> option, the downtime will be in effect between the start and end times you specify.  If you do not select the <i>fixed</i>\n");
		printf("option, Nagios will treat this as \"flexible\" downtime.  Flexible downtime starts when a host goes down or becomes unreachable (sometime between the\n");
		printf("start and end times you specified) and lasts as long as the duration of time you enter.  The duration fields do not apply for fixed dowtime.\n");
		break;

	case CMD_SCHEDULE_HOSTGROUP_SVC_DOWNTIME:
		printf("This command is used to schedule downtime for all services in a particular hostgroup.  During the specified downtime, Nagios will not send notifications out about the services.\n");
		printf("When the scheduled downtime expires, Nagios will send out notifications for the services as it normally would.  Scheduled downtimes are preserved\n");
		printf("across program shutdowns and restarts.  Both the start and end times should be specified in the following format:  <b>mm/dd/yyyy hh:mm:ss</b>.\n");
		printf("If you select the <i>fixed</i> option, the downtime will be in effect between the start and end times you specify.  If you do not select the <i>fixed</i>\n");
		printf("option, Nagios will treat this as \"flexible\" downtime.  Flexible downtime starts when a service enters a non-OK state (sometime between the\n");
		printf("start and end times you specified) and lasts as long as the duration of time you enter.  The duration fields do not apply for fixed dowtime.\n");
		printf("Note that scheduling downtime for services does not automatically schedule downtime for the hosts those services are associated with.  If you want to also schedule downtime for all hosts in the hostgroup, check the 'Schedule downtime for hosts too' option.\n");
		break;

	default:
		printf("Sorry, but no information is available for this command.");
	        }

	printf("</TD></TR>\n");
	printf("</TABLE>\n");

	return;
        }



/* converts a UNIX timestamp to a string we can use */
int time_to_string(time_t *t, char *buffer, int buffer_length){
	struct tm *lt;

	lt=localtime(t);
	snprintf(buffer,buffer_length-1,"%02d/%02d/%04d %02d:%02d:%02d",lt->tm_mon+1,lt->tm_mday,lt->tm_year+1900,lt->tm_hour,lt->tm_min,lt->tm_sec);
	buffer[buffer_length-1]='\x0';

	return OK;
        }


/* converts a time string to a UNIX timestamp */
int string_to_time(char *buffer, time_t *t){
	struct tm lt;

	sscanf(buffer,"%02d/%02d/%04d %02d:%02d:%02d",&lt.tm_mon,&lt.tm_mday,&lt.tm_year,&lt.tm_hour,&lt.tm_min,&lt.tm_sec);

	lt.tm_mon--;
	lt.tm_year-=1900;

	/* tell mktime() to try and compute DST automatically */
	lt.tm_isdst=-1;

	*t=mktime(&lt);

	return OK;
        }