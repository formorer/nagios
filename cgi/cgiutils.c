/***********************************************************************
 *
 * CGIUTILS.C - Common utilities for Nagios CGIs
 * 
 * Copyright (c) 1999-2002 Ethan Galstad (nagios@nagios.org)
 * Last Modified: 04-16-2002
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
 ***********************************************************************/

#include "../common/config.h"
#include "../common/common.h"
#include "../common/locations.h"
#include "../common/objects.h"
#include "../common/statusdata.h"

#include "cgiutils.h"
#include "popen.h"
#include "edata.h"

char            main_config_file[MAX_FILENAME_LENGTH];
char            log_file[MAX_FILENAME_LENGTH];
char            log_archive_path[MAX_FILENAME_LENGTH];
char            command_file[MAX_FILENAME_LENGTH];

char            physical_html_path[MAX_FILENAME_LENGTH];
char            physical_images_path[MAX_FILENAME_LENGTH];
char            physical_ssi_path[MAX_FILENAME_LENGTH];
char            url_html_path[MAX_FILENAME_LENGTH];
char            url_docs_path[MAX_FILENAME_LENGTH];
char            url_context_help_path[MAX_FILENAME_LENGTH];
char            url_images_path[MAX_FILENAME_LENGTH];
char            url_logo_images_path[MAX_FILENAME_LENGTH];
char            url_stylesheets_path[MAX_FILENAME_LENGTH];
char            url_media_path[MAX_FILENAME_LENGTH];

char            *service_critical_sound=NULL;
char            *service_warning_sound=NULL;
char            *service_unknown_sound=NULL;
char            *host_down_sound=NULL;
char            *host_unreachable_sound=NULL;
char            *normal_sound=NULL;
char            *statusmap_background_image=NULL;
char            *statuswrl_include=NULL;

char            nagios_check_command[MAX_INPUT_BUFFER];

char            nagios_process_info[MAX_INPUT_BUFFER];
int             nagios_process_state=STATE_OK;

extern time_t   program_start;
extern int      nagios_pid;
extern int      daemon_mode;
extern int      enable_notifications;
extern int      execute_service_checks;
extern int      accept_passive_service_checks;
extern int      enable_event_handlers;
extern int      obsess_over_services;
extern int      enable_failure_prediction;
extern int      process_performance_data;

time_t          last_command_check=0L;

int             check_external_commands=0;

int             date_format=DATE_FORMAT_US;

int             log_rotation_method=LOG_ROTATION_NONE;
time_t          last_log_rotation=0L;

time_t          this_scheduled_log_rotation=0L;
time_t          last_scheduled_log_rotation=0L;
time_t          next_scheduled_log_rotation=0L;

int             use_authentication=TRUE;

int             interval_length=60;

int             hosts_have_been_read=FALSE;
int             hostgroups_have_been_read=FALSE;
int             contacts_have_been_read=FALSE;
int             contactgroups_have_been_read=FALSE;
int             services_have_been_read=FALSE;
int             timeperiods_have_been_read=FALSE;
int             commands_have_been_read=FALSE;
int             servicedependencies_have_been_read=FALSE;
int             serviceescalations_have_been_read=FALSE;
int             hostgroupescalations_have_been_read=FALSE;
int             hostdependencies_have_been_read=FALSE;
int             hostescalations_have_been_read=FALSE;

int             host_status_has_been_read=FALSE;
int             service_status_has_been_read=FALSE;
int             program_status_has_been_read=FALSE;

int             refresh_rate=DEFAULT_REFRESH_RATE;

int             default_statusmap_layout_method=0;
int             default_statuswrl_layout_method=0;

extern hostgroup       *hostgroup_list;
extern host            *host_list;
extern service         *service_list;
extern contactgroup    *contactgroup_list;
extern command         *command_list;
extern timeperiod      *timeperiod_list;
extern contact         *contact_list;
extern serviceescalation *serviceescalation_list;
extern hostgroupescalation *hostgroupescalation_list;

extern hoststatus      *hoststatus_list;
extern servicestatus   *servicestatus_list;

char            *my_strtok_buffer=NULL;
char            *original_my_strtok_buffer=NULL;

char encoded_url_string[MAX_INPUT_BUFFER];
char encoded_html_string[MAX_INPUT_BUFFER];

#ifdef HAVE_TZNAME
extern char     *tzname[2];
#endif




/**********************************************************
 ***************** CLEANUP FUNCTIONS **********************
 **********************************************************/

/* reset all variables used by the CGIs */
void reset_cgi_vars(void){

	strcpy(main_config_file,"");

	strcpy(physical_html_path,"");
	strcpy(physical_images_path,"");
	strcpy(physical_ssi_path,"");

	strcpy(url_html_path,"");
	strcpy(url_docs_path,"");
	strcpy(url_context_help_path,"");
	strcpy(url_stylesheets_path,"");
	strcpy(url_media_path,"");
	strcpy(url_images_path,"");

	strcpy(log_file,"");
	strcpy(log_archive_path,DEFAULT_LOG_ARCHIVE_PATH);
	if(log_archive_path[strlen(log_archive_path)-1]!='/' && strlen(log_archive_path)<sizeof(log_archive_path)-2)
		strcat(log_archive_path,"/");
	strcpy(command_file,DEFAULT_COMMAND_FILE);

	strcpy(nagios_check_command,"");
	strcpy(nagios_process_info,"");
	nagios_process_state=STATE_OK;

	log_rotation_method=LOG_ROTATION_NONE;

	use_authentication=TRUE;

	interval_length=60;

	refresh_rate=DEFAULT_REFRESH_RATE;

	default_statusmap_layout_method=0;
	default_statusmap_layout_method=0;
	
	return;
        }



/* free all memory for object definitions */
void free_memory(void){

	/* free memory for common object definitions */
	free_object_data();

	/* free memory for status data */
	free_status_data();

	return;
        }




/**********************************************************
 *************** CONFIG FILE FUNCTIONS ********************
 **********************************************************/


/* read the CGI configuration file */
int read_cgi_config_file(char *filename){
	char input_buffer[MAX_INPUT_BUFFER];
	char *temp_buffer;
	FILE *fp;

	fp=fopen(filename,"r");
	if(fp==NULL)
		return ERROR;

	while(read_line(input_buffer,MAX_INPUT_BUFFER,fp)){

	        if(feof(fp))
		        break;

		if(input_buffer==NULL)
		        continue;

		if(strstr(input_buffer,"main_config_file=")==input_buffer){
			temp_buffer=strtok(input_buffer,"=");
			temp_buffer=strtok(NULL,"\x0");
			if(temp_buffer!=NULL){
				strncpy(main_config_file,temp_buffer,sizeof(main_config_file));
				main_config_file[sizeof(main_config_file)-1]='\x0';
				strip(main_config_file);
			        }
		        }

		else if((strstr(input_buffer,"use_authentication=")==input_buffer)){
			temp_buffer=strtok(input_buffer,"=");
			temp_buffer=strtok(NULL,"\n");
			if(temp_buffer==NULL)
				use_authentication=TRUE;
			else if(atoi(temp_buffer)==0)
				use_authentication=FALSE;
			else
				use_authentication=TRUE;
		        }

		else if(strstr(input_buffer,"nagios_check_command=")==input_buffer){
			temp_buffer=strtok(input_buffer,"=");
			temp_buffer=strtok(NULL,"\x0");
			if(temp_buffer!=NULL){
				strncpy(nagios_check_command,temp_buffer,sizeof(nagios_check_command));
				nagios_check_command[sizeof(nagios_check_command)-1]='\x0';
				strip(nagios_check_command);
			        }
		        }

		else if(strstr(input_buffer,"refresh_rate=")==input_buffer){
			temp_buffer=strtok(input_buffer,"=");
			temp_buffer=strtok(NULL,"\x0");
			refresh_rate=atoi((temp_buffer==NULL)?"60":temp_buffer);
		        }

		else if(strstr(input_buffer,"physical_html_path=")==input_buffer){
			temp_buffer=strtok(input_buffer,"=");
			temp_buffer=strtok(NULL,"\n");
			strncpy(physical_html_path,(temp_buffer==NULL)?"":temp_buffer,sizeof(physical_html_path));
			physical_html_path[sizeof(physical_html_path)-1]='\x0';
			strip(physical_html_path);
			if(physical_html_path[strlen(physical_html_path)-1]!='/' && (strlen(physical_html_path) < sizeof(physical_html_path)-1))
				strcat(physical_html_path,"/");

			snprintf(physical_images_path,sizeof(physical_images_path),"%simages/",physical_html_path);
			physical_images_path[sizeof(physical_images_path)-1]='\x0';

			snprintf(physical_ssi_path,sizeof(physical_images_path),"%sssi/",physical_html_path);
			physical_ssi_path[sizeof(physical_ssi_path)-1]='\x0';
		        }

		else if(strstr(input_buffer,"url_html_path=")==input_buffer){
			temp_buffer=strtok(input_buffer,"=");
			temp_buffer=strtok(NULL,"\n");

			strncpy(url_html_path,(temp_buffer==NULL)?"":temp_buffer,sizeof(url_html_path));
			url_html_path[sizeof(url_html_path)-1]='\x0';

			strip(url_html_path);
			if(url_html_path[strlen(url_html_path)-1]!='/' && (strlen(url_html_path) < sizeof(url_html_path)-1))
				strcat(url_html_path,"/");

			snprintf(url_docs_path,sizeof(url_docs_path),"%sdocs/",url_html_path);
			url_docs_path[sizeof(url_docs_path)-1]='\x0';

			snprintf(url_context_help_path,sizeof(url_context_help_path),"%scontexthelp/",url_html_path);
			url_context_help_path[sizeof(url_context_help_path)-1]='\x0';

			snprintf(url_images_path,sizeof(url_images_path),"%simages/",url_html_path);
			url_images_path[sizeof(url_images_path)-1]='\x0';

			snprintf(url_logo_images_path,sizeof(url_logo_images_path),"%slogos/",url_images_path);
			url_logo_images_path[sizeof(url_logo_images_path)-1]='\x0';

			snprintf(url_stylesheets_path,sizeof(url_stylesheets_path),"%sstylesheets/",url_html_path);
			url_stylesheets_path[sizeof(url_stylesheets_path)-1]='\x0';

			snprintf(url_media_path,sizeof(url_media_path),"%smedia/",url_html_path);
			url_media_path[sizeof(url_media_path)-1]='\x0';
		        }

		else if(strstr(input_buffer,"service_critical_sound=")==input_buffer){
			temp_buffer=strtok(input_buffer,"=");
			temp_buffer=strtok(NULL,"\n");
			if(temp_buffer==NULL)
				continue;
			service_critical_sound=(char *)malloc(strlen(temp_buffer)+1);
			if(service_critical_sound!=NULL)
				strcpy(service_critical_sound,temp_buffer);
		        }

		else if(strstr(input_buffer,"service_warning_sound=")==input_buffer){
			temp_buffer=strtok(input_buffer,"=");
			temp_buffer=strtok(NULL,"\n");
			if(temp_buffer==NULL)
				continue;
			service_warning_sound=(char *)malloc(strlen(temp_buffer)+1);
			if(service_warning_sound!=NULL)
				strcpy(service_warning_sound,temp_buffer);
		        }

		else if(strstr(input_buffer,"service_unknown_sound=")==input_buffer){
			temp_buffer=strtok(input_buffer,"=");
			temp_buffer=strtok(NULL,"\n");
			if(temp_buffer==NULL)
				continue;
			service_unknown_sound=(char *)malloc(strlen(temp_buffer)+1);
			if(service_unknown_sound!=NULL)
				strcpy(service_unknown_sound,temp_buffer);
		        }

		else if(strstr(input_buffer,"host_down_sound=")==input_buffer){
			temp_buffer=strtok(input_buffer,"=");
			temp_buffer=strtok(NULL,"\n");
			if(temp_buffer==NULL)
				continue;
			host_down_sound=(char *)malloc(strlen(temp_buffer)+1);
			if(host_down_sound!=NULL)
				strcpy(host_down_sound,temp_buffer);
		        }

		else if(strstr(input_buffer,"host_unreachable_sound=")==input_buffer){
			temp_buffer=strtok(input_buffer,"=");
			temp_buffer=strtok(NULL,"\n");
			if(temp_buffer==NULL)
				continue;
			host_unreachable_sound=(char *)malloc(strlen(temp_buffer)+1);
			if(host_unreachable_sound!=NULL)
				strcpy(host_unreachable_sound,temp_buffer);
		        }

		else if(strstr(input_buffer,"normal_sound=")==input_buffer){
			temp_buffer=strtok(input_buffer,"=");
			temp_buffer=strtok(NULL,"\n");
			if(temp_buffer==NULL)
				continue;
			normal_sound=(char *)malloc(strlen(temp_buffer)+1);
			if(normal_sound!=NULL)
				strcpy(normal_sound,temp_buffer);
		        }

		else if(strstr(input_buffer,"statusmap_background_image=")==input_buffer){
			temp_buffer=strtok(input_buffer,"=");
			temp_buffer=strtok(NULL,"\n");
			if(temp_buffer==NULL)
				continue;
			statusmap_background_image=(char *)malloc(strlen(temp_buffer)+1);
			if(statusmap_background_image!=NULL)
				strcpy(statusmap_background_image,temp_buffer);
		        }

		else if(strstr(input_buffer,"default_statusmap_layout=")==input_buffer){
			temp_buffer=strtok(input_buffer,"=");
			temp_buffer=strtok(NULL,"\x0");
			default_statusmap_layout_method=atoi((temp_buffer==NULL)?"0":temp_buffer);
		        }

		else if(strstr(input_buffer,"default_statuswrl_layout=")==input_buffer){
			temp_buffer=strtok(input_buffer,"=");
			temp_buffer=strtok(NULL,"\x0");
			default_statuswrl_layout_method=atoi((temp_buffer==NULL)?"0":temp_buffer);
		        }

		else if(strstr(input_buffer,"statuswrl_include=")==input_buffer){
			temp_buffer=strtok(input_buffer,"=");
			temp_buffer=strtok(NULL,"\n");
			if(temp_buffer==NULL)
				continue;
			statuswrl_include=(char *)malloc(strlen(temp_buffer)+1);
			if(statuswrl_include!=NULL)
				strcpy(statuswrl_include,temp_buffer);
		        }
 	        }

	fclose(fp);

	if(!strcmp(main_config_file,""))
		return ERROR;
	else
		return OK;
        }



/* read the main configuration file */
int read_main_config_file(char *filename){
	char input_buffer[MAX_INPUT_BUFFER];
	char *temp_buffer;
	FILE *fp;

	
	fp=fopen(filename,"r");
	if(fp==NULL)
		return ERROR;

	while(read_line(input_buffer,MAX_INPUT_BUFFER,fp)){

	        if(feof(fp))
		        break;

		if(input_buffer==NULL)
		        continue;

		if(strstr(input_buffer,"interval_length=")==input_buffer){
			temp_buffer=strtok(input_buffer,"=");
			temp_buffer=strtok(NULL,"\x0");
			interval_length=(temp_buffer==NULL)?60:atoi(temp_buffer);
		        }

		else if(strstr(input_buffer,"log_file=")==input_buffer){
			temp_buffer=strtok(input_buffer,"=");
			temp_buffer=strtok(NULL,"\x0");
			strncpy(log_file,(temp_buffer==NULL)?"":temp_buffer,sizeof(log_file));
			log_file[sizeof(log_file)-1]='\x0';
			strip(log_file);
		        }

		else if(strstr(input_buffer,"log_archive_path=")==input_buffer){
			temp_buffer=strtok(input_buffer,"=");
			temp_buffer=strtok(NULL,"\n");
			strncpy(log_archive_path,(temp_buffer==NULL)?"":temp_buffer,sizeof(log_archive_path));
			log_archive_path[sizeof(log_archive_path)-1]='\x0';
			strip(physical_html_path);
			if(log_archive_path[strlen(log_archive_path)-1]!='/' && (strlen(log_archive_path) < sizeof(log_archive_path)-1))
				strcat(log_archive_path,"/");
		        }

		else if(strstr(input_buffer,"log_rotation_method=")==input_buffer){
			temp_buffer=strtok(input_buffer,"=");
			temp_buffer=strtok(NULL,"\x0");
			if(temp_buffer==NULL)
				log_rotation_method=LOG_ROTATION_NONE;
			else if(!strcmp(temp_buffer,"h"))
				log_rotation_method=LOG_ROTATION_HOURLY;
			else if(!strcmp(temp_buffer,"d"))
				log_rotation_method=LOG_ROTATION_DAILY;
			else if(!strcmp(temp_buffer,"w"))
				log_rotation_method=LOG_ROTATION_WEEKLY;
			else if(!strcmp(temp_buffer,"m"))
				log_rotation_method=LOG_ROTATION_MONTHLY;
		        }

		else if(strstr(input_buffer,"command_file=")==input_buffer){
			temp_buffer=strtok(input_buffer,"=");
			temp_buffer=strtok(NULL,"\x0");
			strncpy(command_file,(temp_buffer==NULL)?"":temp_buffer,sizeof(command_file));
			command_file[sizeof(command_file)-1]='\x0';
			strip(command_file);
		        }

		else if(strstr(input_buffer,"check_external_commands=")==input_buffer){
			temp_buffer=strtok(input_buffer,"=");
			temp_buffer=strtok(NULL,"\x0");
			check_external_commands=(temp_buffer==NULL)?0:atoi(temp_buffer);
		        }

		else if(strstr(input_buffer,"date_format=")==input_buffer){
			temp_buffer=strtok(input_buffer,"=");
			temp_buffer=strtok(NULL,"\x0");
			if(temp_buffer==NULL)
				date_format=DATE_FORMAT_US;
			else if(!strcmp(temp_buffer,"euro"))
				date_format=DATE_FORMAT_EURO;
			else if(!strcmp(temp_buffer,"iso8601"))
				date_format=DATE_FORMAT_ISO8601;
			else if(!strcmp(temp_buffer,"strict-iso8601"))
				date_format=DATE_FORMAT_STRICT_ISO8601;
			else
				date_format=DATE_FORMAT_US;
		        }
               }

	fclose(fp);

	return OK;
        }



/* read all object definitions */
int read_all_object_configuration_data(char *config_file,int options){

	/* don't duplicate things we've already read in */
	if(hosts_have_been_read==TRUE && (options & READ_HOSTS))
		options-=READ_HOSTS;
	if(hostgroups_have_been_read==TRUE && (options & READ_HOSTGROUPS))
		options-=READ_HOSTGROUPS;
	if(contacts_have_been_read==TRUE && (options & READ_CONTACTS))
		options-=READ_CONTACTS;
	if(contactgroups_have_been_read==TRUE && (options & READ_CONTACTGROUPS))
		options-=READ_CONTACTGROUPS;
	if(services_have_been_read==TRUE && (options & READ_SERVICES))
		options-=READ_SERVICES;
	if(timeperiods_have_been_read==TRUE && (options & READ_TIMEPERIODS))
		options-=READ_TIMEPERIODS;
	if(commands_have_been_read==TRUE && (options & READ_COMMANDS))
		options-=READ_COMMANDS;
	if(servicedependencies_have_been_read==TRUE && (options & READ_SERVICEDEPENDENCIES))
		options-=READ_SERVICEDEPENDENCIES;
	if(serviceescalations_have_been_read==TRUE && (options & READ_SERVICEESCALATIONS))
		options-=READ_SERVICEESCALATIONS;
	if(hostgroupescalations_have_been_read==TRUE && (options & READ_HOSTGROUPESCALATIONS))
		options-=READ_HOSTGROUPESCALATIONS;
	if(hostdependencies_have_been_read==TRUE && (options & READ_HOSTDEPENDENCIES))
		options-=READ_HOSTDEPENDENCIES;
	if(hostescalations_have_been_read==TRUE && (options & READ_HOSTESCALATIONS))
		options-=READ_HOSTESCALATIONS;

	/* read in all external config data of the desired type(s) */
	read_object_config_data(config_file,options);

	/* mark what items we've read in... */
	if(options & READ_HOSTS)
		hosts_have_been_read=TRUE;
	if(options & READ_HOSTGROUPS)
		hostgroups_have_been_read=TRUE;
	if(options & READ_CONTACTS)
		contacts_have_been_read=TRUE;
	if(options & READ_CONTACTGROUPS)
		contactgroups_have_been_read=TRUE;
	if(options & READ_SERVICES)
		services_have_been_read=TRUE;
	if(options & READ_TIMEPERIODS)
		timeperiods_have_been_read=TRUE;
	if(options & READ_COMMANDS)
		commands_have_been_read=TRUE;
	if(options & READ_SERVICEDEPENDENCIES)
		servicedependencies_have_been_read=TRUE;
	if(options & READ_SERVICEESCALATIONS)
		serviceescalations_have_been_read=TRUE;
	if(options & READ_HOSTGROUPESCALATIONS)
		hostgroupescalations_have_been_read=TRUE;
	if(options & READ_HOSTDEPENDENCIES)
		hostdependencies_have_been_read=TRUE;
	if(options & READ_HOSTESCALATIONS)
		hostescalations_have_been_read=TRUE;

	return OK;
        }


/* read all status data */
int read_all_status_data(char *config_file,int options){
	int result=OK;

	/* don't duplicate things we've already read in */
	if(program_status_has_been_read==TRUE && (options & READ_PROGRAM_STATUS))
		options-=READ_PROGRAM_STATUS;
	if(host_status_has_been_read==TRUE && (options & READ_HOST_STATUS))
		options-=READ_HOST_STATUS;
	if(service_status_has_been_read==TRUE && (options & READ_SERVICE_STATUS))
		options-=READ_SERVICE_STATUS;

	/* read in all external status data */
	result=read_status_data(config_file,options);

	/* mark what items we've read in... */
	if(options & READ_PROGRAM_STATUS)
		program_status_has_been_read=TRUE;
	if(options & READ_HOST_STATUS)
		host_status_has_been_read=TRUE;
	if(options & READ_SERVICE_STATUS)
		service_status_has_been_read=TRUE;

	return result;
        }



/**********************************************************
 *************** MISC UTILITY FUNCTIONS *******************
 **********************************************************/


/* strips newline characters, spaces, tabs, and carriage returns from the end of a string */
void strip(char *buffer){
	int x;

	if(buffer==NULL)
		return;

	x=(int)strlen(buffer);

	for(;x>=1;x--){
		if(buffer[x-1]==' ' || buffer[x-1]=='\n' || buffer[x-1]=='\r' || buffer[x-1]=='\t' || buffer[x-1]==13)
			buffer[x-1]='\x0';
		else
			break;
	        }

	return;
        }


/* strips HTML and bad stuff from plugin output */
void sanitize_plugin_output(char *buffer){
	int x=0;
	int y=0;
	int in_html=FALSE;
	char *new_buffer;

	if(buffer==NULL)
		return;

	new_buffer=strdup(buffer);
	if(new_buffer==NULL)
		return;

	/* check each character */
	for(x=0,y=0;buffer[x]!='\x0';x++){

		/* we just started an HTML tag */
		if(buffer[x]=='<'){
			in_html=TRUE;
			continue;
		        }

		/* end of an HTML tage */
		else if(buffer[x]=='>'){
			in_html=FALSE;
			continue;
		        }

		/* skip everything inside HTML tags */
		else if(in_html==TRUE)
			continue;

		/* strip single and double quotes */
		else if(buffer[x]=='\'' || buffer[x]=='\"')
			new_buffer[y++]=' ';

		/* strip semicolons (replace with colons) */
		else if(buffer[x]==';')
			new_buffer[y++]=':';

		/* strip pipe and ampersand */
		else if(buffer[x]=='&' || buffer[x]=='|')
			new_buffer[y++]=' ';

		/* normal character */
		else
			new_buffer[y++]=buffer[x];
	        }

	/* terminate sanitized buffer */
	new_buffer[y++]='\x0';

	/* copy the sanitized buffer back to the original */
	strcpy(buffer,new_buffer);

	/* free memory allocated to the new buffer */
	free(new_buffer);

	return;
        }
	


/* reads a line of text from a file and strips it clean */
char * read_raw_line(char *buffer, int maxlength, FILE *fp){
	char *result;

	/* clear the buffer */
	strcpy(buffer,"");

	/* read in one line of text */
	result=fgets(buffer,maxlength-1,fp);

	/* if we were able to read something, strip extra characters from the end */
	if(result){
		buffer[maxlength-1]='\x0';
		strip(buffer);
	        }

	/* return the number of characters read */
	return result;
        }


/* read a line of text from a file, skipping comments and empty lines */
char * read_line(char *buffer, int maxlength, FILE *fp){
	char *result;
	int keep_line=TRUE;

	while((result=read_raw_line(buffer,maxlength,fp))){

		if(buffer[0]=='#')
			keep_line=FALSE;
		else if(buffer[0]=='\r')
			keep_line=FALSE;
		else if(buffer[0]=='\n')
			keep_line=FALSE;
		else if(buffer[0]=='\x0')
			keep_line=FALSE;
		else
			keep_line=TRUE;

		if(keep_line==TRUE)
			break;
	        }

	return result;
        }



/* get date/time string */
void get_time_string(time_t *raw_time,char *buffer,int buffer_length,int type){
	time_t t;
	struct tm *tm_ptr;
	int day;
	int hour;
	int minute;
	int second;
	int year;
	char *weekdays[7]={"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
	char *months[12]={"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sept","Oct","Nov","Dec"};
	char *tzone="";

	if(raw_time==NULL)
		time(&t);
	else 
		t=*raw_time;

	if(type==HTTP_DATE_TIME)
		tm_ptr=gmtime(&t);
	else
		tm_ptr=localtime(&t);

	hour=tm_ptr->tm_hour;
	minute=tm_ptr->tm_min;
	second=tm_ptr->tm_sec;
	day=tm_ptr->tm_mday;
	year=tm_ptr->tm_year+1900;

#ifdef HAVE_TM_ZONE
	tzone=(char *)tm_ptr->tm_zone;
#else
	tzone=(tm_ptr->tm_isdst)?tzname[1]:tzname[0];
#endif

	/* ctime() style */
	if(type==LONG_DATE_TIME)
		snprintf(buffer,buffer_length,"%s %s %d %02d:%02d:%02d %s %d",weekdays[tm_ptr->tm_wday],months[tm_ptr->tm_mon],day,hour,minute,second,tzone,year);

	/* short style */
	else if(type==SHORT_DATE_TIME){
		if(date_format==DATE_FORMAT_EURO)
			snprintf(buffer,buffer_length,"%02d-%02d-%04d %02d:%02d:%02d",tm_ptr->tm_mday,tm_ptr->tm_mon+1,tm_ptr->tm_year+1900,tm_ptr->tm_hour,tm_ptr->tm_min,tm_ptr->tm_sec);
		else if(date_format==DATE_FORMAT_ISO8601 || date_format==DATE_FORMAT_STRICT_ISO8601)
			snprintf(buffer,buffer_length,"%04d-%02d-%02d%c%02d:%02d:%02d",tm_ptr->tm_year+1900,tm_ptr->tm_mon+1,tm_ptr->tm_mday,(date_format==DATE_FORMAT_STRICT_ISO8601)?'T':' ',tm_ptr->tm_hour,tm_ptr->tm_min,tm_ptr->tm_sec);
		else
			snprintf(buffer,buffer_length,"%02d-%02d-%04d %02d:%02d:%02d",tm_ptr->tm_mon+1,tm_ptr->tm_mday,tm_ptr->tm_year+1900,tm_ptr->tm_hour,tm_ptr->tm_min,tm_ptr->tm_sec);
	        }

	/* expiration date/time for HTTP headers */
	else if(type==HTTP_DATE_TIME)
		snprintf(buffer,buffer_length,"%s, %02d %s %d %02d:%02d:%02d GMT",weekdays[tm_ptr->tm_wday],day,months[tm_ptr->tm_mon],year,hour,minute,second);

	buffer[buffer_length-1]='\x0';

	return;
        }


/* get time string for an interval of time */
void get_interval_time_string(int time_units,char *buffer,int buffer_length){
	unsigned long total_seconds;
	int hours=0;
	int minutes=0;
	int seconds=0;

	total_seconds=(unsigned long)(time_units*interval_length);
	hours=(int)total_seconds/3600;
	total_seconds%=3600;
	minutes=(int)total_seconds/60;
	total_seconds%=60;
	seconds=(int)total_seconds;
	snprintf(buffer,buffer_length,"%dh %dm %ds",hours,minutes,seconds);
	buffer[buffer_length-1]='\x0';

	return;
        }


/* fix the problem with strtok() skipping empty options between tokens */	
char *my_strtok(char *buffer,char *tokens){
	char *token_position;
	char *sequence_head;

	if(buffer!=NULL){
		if(original_my_strtok_buffer!=NULL)
			free(original_my_strtok_buffer);
		my_strtok_buffer=malloc(strlen(buffer)+1);
		if(my_strtok_buffer==NULL)
			return NULL;
		original_my_strtok_buffer=my_strtok_buffer;
		strcpy(my_strtok_buffer,buffer);
	        }
	
	sequence_head=my_strtok_buffer;

	if(sequence_head[0]=='\x0')
		return NULL;
	
	token_position=index(my_strtok_buffer,tokens[0]);

	if(token_position==NULL){
		my_strtok_buffer=index(my_strtok_buffer,'\x0');
		return sequence_head;
	        }

	token_position[0]='\x0';
	my_strtok_buffer=token_position+1;

	return sequence_head;

        }


/* fixes compiler problems under Solaris, since strsep() isn't included */
/* this code is taken from the glibc source */
char *my_strsep (char **stringp, const char *delim){
	char *begin, *end;

	begin = *stringp;
	if (begin == NULL)
		return NULL;

	/* A frequent case is when the delimiter string contains only one
	   character.  Here we don't need to call the expensive `strpbrk'
	   function and instead work using `strchr'.  */
	if(delim[0]=='\0' || delim[1]=='\0'){
		char ch = delim[0];

		if(ch=='\0')
			end=NULL;
		else{
			if(*begin==ch)
				end=begin;
			else
				end=strchr(begin+1,ch);
			}
		}

	else
		/* Find the end of the token.  */
		end = strpbrk (begin, delim);

	if(end){

		/* Terminate the token and set *STRINGP past NUL character.  */
		*end++='\0';
		*stringp=end;
		}
	else
		/* No more delimiters; this is the last token.  */
		*stringp=NULL;

	return begin;
	}


/* get days, hours, minutes, and seconds from a raw time_t format or total seconds */
void get_time_breakdown(unsigned long raw_time,int *days,int *hours,int *minutes,int *seconds){
	unsigned long temp_time;
	int temp_days;
	int temp_hours;
	int temp_minutes;
	int temp_seconds;

	temp_time=raw_time;

	temp_days=temp_time/86400;
	temp_time-=(temp_days * 86400);
	temp_hours=temp_time/3600;
	temp_time-=(temp_hours * 3600);
	temp_minutes=temp_time/60;
	temp_time-=(temp_minutes * 60);
	temp_seconds=(int)temp_time;

	*days=temp_days;
	*hours=temp_hours;
	*minutes=temp_minutes;
	*seconds=temp_seconds;

	return;
	}



/* encodes a string in proper URL format */
char * url_encode(char *input){
	int len,output_len;
	int x,y;
	char temp_expansion[4];

	len=(int)strlen(input);
	output_len=(int)sizeof(encoded_url_string);

	encoded_url_string[0]='\x0';

	for(x=0,y=0;x<=len && y<output_len-1;x++){

		/* end of string */
		if((char)input[x]==(char)'\x0'){
			encoded_url_string[y]='\x0';
			break;
		        }

		/* alpha-numeric characters don't get encoded */
		else if(((char)input[x]>='0' && (char)input[x]<='9') || ((char)input[x]>='A' && (char)input[x]<='Z') || ((char)input[x]>=(char)'a' && (char)input[x]<=(char)'z')){
			encoded_url_string[y]=input[x];
			y++;
		        }

		/* spaces are pluses */
		else if((char)input[x]<=(char)' '){
			encoded_url_string[y]='+';
			y++;
		        }

		/* anything else gets represented by its hex value */
		else{
			encoded_url_string[y]='\x0';
			if((int)strlen(encoded_url_string)<(output_len-3)){
				sprintf(temp_expansion,"%%%02X",(unsigned int)input[x]);
				strcat(encoded_url_string,temp_expansion);
				y+=3;
			        }
		        }
	        }

	encoded_url_string[sizeof(encoded_url_string)-1]='\x0';

	return &encoded_url_string[0];
        }



/* escapes a string used in HTML */
char * html_encode(char *input){
	int len,output_len;
	int x,y;
	char temp_expansion[7];

	len=(int)strlen(input);
	output_len=(int)sizeof(encoded_html_string);

	encoded_html_string[0]='\x0';

	for(x=0,y=0;x<=len && y<output_len-1;x++){

		/* end of string */
		if((char)input[x]==(char)'\x0'){
			encoded_html_string[y]='\x0';
			break;
		        }

		/* alpha-numeric characters don't get encoded */
		else if(((char)input[x]>='0' && (char)input[x]<='9') || ((char)input[x]>='A' && (char)input[x]<='Z') || ((char)input[x]>=(char)'a' && (char)input[x]<=(char)'z')){
			encoded_html_string[y]=input[x];
			y++;
		        }

		/* spaces are encoded as non-breaking spaces */
		else if((char)input[x]<=(char)' '){

			encoded_html_string[y]='\x0';
			if((int)strlen(encoded_html_string)<(output_len-6)){
				strcat(encoded_html_string,"&nbsp;");
				y+=6;
			        }
		        }

		/* for simplicity, everything else gets represented by its numeric value */
		else{
			encoded_html_string[y]='\x0';
			sprintf(temp_expansion,"&#%d;",(unsigned int)input[x]);
			if((int)strlen(encoded_html_string)<(output_len-strlen(temp_expansion))){
				strcat(encoded_html_string,temp_expansion);
				y+=strlen(temp_expansion);
			        }
		        }
	        }

	encoded_html_string[sizeof(encoded_html_string)-1]='\x0';

	return &encoded_html_string[0];
        }



/* determines the log file we should use (from current time) */
void get_log_archive_to_use(int archive,char *buffer,int buffer_length){
	struct tm *t;

	/* determine the time at which the log was rotated for this archive # */
	determine_log_rotation_times(archive);

	/* if we're not rotating the logs or if we want the current log, use the main one... */
	if(log_rotation_method==LOG_ROTATION_NONE || archive<=0){
		strncpy(buffer,log_file,buffer_length);
		buffer[buffer_length-1]='\x0';
		return;
	        }

	t=localtime(&this_scheduled_log_rotation);

	/* use the time that the log rotation occurred to figure out the name of the log file */
	snprintf(buffer,buffer_length,"%snagios-%02d-%02d-%d-%02d.log",log_archive_path,t->tm_mon+1,t->tm_mday,t->tm_year+1900,t->tm_hour);
	buffer[buffer_length-1]='\x0';

	return;
        }



/* determines log archive to use, given a specific time */
int determine_archive_to_use_from_time(time_t target_time){
	time_t current_time;
	int current_archive=0;

	/* if log rotation is disabled, we don't have archives */
	if(log_rotation_method==LOG_ROTATION_NONE)
		return 0;

	/* make sure target time is rational */
	current_time=time(NULL);
	if(target_time>=current_time)
		return 0;

	/* backtrack through archives to find the one we need for this time */
	/* start with archive of 1, subtract one when we find the right time period to compensate for current (non-rotated) log */
	for(current_archive=1;;current_archive++){

		/* determine time at which the log rotation occurred for this archive number */
		determine_log_rotation_times(current_archive);

		/* if the target time falls within the times encompassed by this archive, we have the right archive! */
		if(target_time>=this_scheduled_log_rotation)
			return current_archive-1;
	        }

	return 0;
        }



/* determines the log rotation times - past, present, future */
void determine_log_rotation_times(int archive){
	struct tm *t;
	int current_month;
	int is_dst_now=FALSE;
	time_t current_time;

	/* negative archive numbers don't make sense */
	/* if archive=0 (current log), this_scheduled_log_rotation time is set to next rotation time */
	if(archive<0)
		return;

	time(&current_time);
	t=localtime(&current_time);
	is_dst_now=(t->tm_isdst>0)?TRUE:FALSE;
	t->tm_min=0;
	t->tm_sec=0;


	switch(log_rotation_method){

	case LOG_ROTATION_HOURLY:
		this_scheduled_log_rotation=mktime(t);
		this_scheduled_log_rotation=(time_t)(this_scheduled_log_rotation-((archive-1)*3600));
		last_scheduled_log_rotation=(time_t)(this_scheduled_log_rotation-3600);
		break;

	case LOG_ROTATION_DAILY:
		t->tm_hour=0;
		this_scheduled_log_rotation=mktime(t);
		this_scheduled_log_rotation=(time_t)(this_scheduled_log_rotation-((archive-1)*86400));
		last_scheduled_log_rotation=(time_t)(this_scheduled_log_rotation-86400);
		break;

	case LOG_ROTATION_WEEKLY:
		t->tm_hour=0;
		this_scheduled_log_rotation=mktime(t);
		this_scheduled_log_rotation=(time_t)(this_scheduled_log_rotation-(86400*t->tm_wday));
		this_scheduled_log_rotation=(time_t)(this_scheduled_log_rotation-((archive-1)*604800));
		last_scheduled_log_rotation=(time_t)(this_scheduled_log_rotation-604800);
		break;

	case LOG_ROTATION_MONTHLY:

		t=localtime(&current_time);
		t->tm_mon++;
		t->tm_mday=1;
		t->tm_hour=0;
		t->tm_min=0;
		t->tm_sec=0;
		for(current_month=0;current_month<=archive;current_month++){
			if(t->tm_mon==0){
				t->tm_mon=11;
				t->tm_year--;
			        }
			else
				t->tm_mon--;
		        }
		last_scheduled_log_rotation=mktime(t);

		t=localtime(&current_time);
		t->tm_mon++;
		t->tm_mday=1;
		t->tm_hour=0;
		t->tm_min=0;
		t->tm_sec=0;
		for(current_month=0;current_month<archive;current_month++){
			if(t->tm_mon==0){
				t->tm_mon=11;
				t->tm_year--;
			        }
			else
				t->tm_mon--;
		        }
		this_scheduled_log_rotation=mktime(t);

		break;
	default:
		break;
	        }

	/* adust this rotation time for daylist savings time */
	t=localtime(&this_scheduled_log_rotation);
	if(t->tm_isdst>0 && is_dst_now==FALSE)
		this_scheduled_log_rotation=(time_t)(this_scheduled_log_rotation-3600);
	else if(t->tm_isdst==0 && is_dst_now==TRUE)
		this_scheduled_log_rotation=(time_t)(this_scheduled_log_rotation+3600);

	/* adust last rotation time for daylist savings time */
	t=localtime(&last_scheduled_log_rotation);
	if(t->tm_isdst>0 && is_dst_now==FALSE)
		last_scheduled_log_rotation=(time_t)(last_scheduled_log_rotation-3600);
	else if(t->tm_isdst==0 && is_dst_now==TRUE)
		last_scheduled_log_rotation=(time_t)(last_scheduled_log_rotation+3600);

	return;
        }



/* get the status of the nagios process */
int get_nagios_process_info(void){
	char input_buffer[MAX_INPUT_BUFFER];
	int process_state=STATE_OK;
	int result=STATE_OK;
	FILE *fp;

	/* if there is no check command defined, we don't know what's going on - assume its okay */
	if(!strcmp(nagios_check_command,"")){
		process_state=STATE_OK;
		strcpy(nagios_process_info,"No process check command has been defined in your CGI configuration file.<BR>Nagios process is assumed to be running properly.");
	        }

	/* else run the check... */
	else{

		/* clear out the old check results */
		strcpy(nagios_process_info,"");
		strcpy(input_buffer,"");

		/* run the process check command */
		fp=spopen(nagios_check_command);
		if(fp==NULL){
			process_state=STATE_UNKNOWN;
			strncpy(nagios_process_info,"Could not open pipe for process check command",sizeof(nagios_process_info));
			nagios_process_info[sizeof(nagios_process_info)-1]='\x0';
	                }
		else{
			/* grab command output */
			fgets(input_buffer,MAX_INPUT_BUFFER-1,fp);
			result=spclose(fp);

			/* get the program return code */
			process_state=WEXITSTATUS(result);

			/* do some basic bounds checking on the return code */
			if(process_state<-1 || process_state>2)
				process_state=STATE_UNKNOWN;

			/* check the output from the command */
			if(!strcmp(input_buffer,"")){
				process_state=STATE_UNKNOWN;
				strncpy(input_buffer,"Nagios check command did not return any output",sizeof(input_buffer));
				input_buffer[sizeof(input_buffer)-1]='\x0';
			        }

			/* store output for later use */
			strncpy(nagios_process_info,input_buffer,sizeof(nagios_process_info));
			nagios_process_info[sizeof(nagios_process_info)-1]='\x0';
		        }
	        }

	nagios_process_state=process_state;

	return process_state;
        }


/* calculates host state times */
void calculate_host_state_times(char *host_name,unsigned long *tmt, unsigned long *tup, float *ptup, unsigned long *tdown, float *ptdown, unsigned long *tunreachable, float *ptunreachable){
	time_t current_time;
	unsigned long total_program_time;
	unsigned long time_difference;
	unsigned long total_monitored_time;
	unsigned long time_up;
	unsigned long time_down;
	unsigned long time_unreachable;
	float percent_time_up;
	float percent_time_down;
	float percent_time_unreachable;
	hoststatus *temp_hoststatus;

	/* get host status info */
	temp_hoststatus=find_hoststatus(host_name);

	/* get the current time */
	time(&current_time);

	/* get total running time for Nagios */
	total_program_time=(unsigned long)(program_start>current_time)?0:(current_time-program_start);

	if((unsigned long)temp_hoststatus->last_state_change==0L || temp_hoststatus->last_state_change<program_start)
		time_difference=total_program_time;
	else
		time_difference=(unsigned long)(temp_hoststatus->last_state_change>current_time)?0:(current_time-temp_hoststatus->last_state_change);

	
	time_up=temp_hoststatus->time_up;
	if(temp_hoststatus->status==HOST_UP)
		time_up+=time_difference;
	time_down=temp_hoststatus->time_down;
	if(temp_hoststatus->status==HOST_DOWN)
		time_down+=time_difference;
	time_unreachable=temp_hoststatus->time_unreachable;
	if(temp_hoststatus->status==HOST_UNREACHABLE)
		time_unreachable+=time_difference;

	total_monitored_time=time_up+time_down+time_unreachable;

	if(total_monitored_time==0L){
		percent_time_up=0.0;
		percent_time_down=0.0;
		percent_time_unreachable=0.0;
	        }
	else{
		percent_time_up=(float)(((float)time_up/(float)total_monitored_time)*100.0);
		percent_time_down=(float)(((float)time_down/(float)total_monitored_time)*100.0);
		percent_time_unreachable=(float)(((float)time_unreachable/(float)total_monitored_time)*100.0);
	        }

	*tmt=total_monitored_time;
	*tup=time_up;
	*ptup=percent_time_up;
	*tdown=time_down;
	*ptdown=percent_time_down;
	*tunreachable=time_unreachable;
	*ptunreachable=percent_time_unreachable;

	return;
        }


/* calculates service state times */
void calculate_service_state_times(char *host_name,char *service_desc,unsigned long *tmt, unsigned long *tok, float *ptok, unsigned long *twarn, float *ptwarn, unsigned long *tun, float *ptun, unsigned long *tcrit, float *ptcrit){
	time_t current_time;
	unsigned long total_program_time;
	unsigned long time_difference;
	unsigned long total_monitored_time;
	unsigned long time_ok;
	unsigned long time_warning;
	unsigned long time_unknown;
	unsigned long time_critical;
	float percent_time_ok;
	float percent_time_warning;
	float percent_time_unknown;
	float percent_time_critical;
	servicestatus *temp_svcstatus;

	/* get service status info */
	temp_svcstatus=find_servicestatus(host_name,service_desc);

	/* get the current time */
	time(&current_time);

	/* get total running time for Nagios */
	total_program_time=(unsigned long)(program_start>current_time)?0:(current_time-program_start);

	if((unsigned long)temp_svcstatus->last_state_change==0L || temp_svcstatus->last_state_change<program_start)
		time_difference=total_program_time;
	else
		time_difference=(unsigned long)(temp_svcstatus->last_state_change>current_time)?0:(current_time-temp_svcstatus->last_state_change);

	
	time_ok=temp_svcstatus->time_ok;
	time_warning=temp_svcstatus->time_warning;
	time_unknown=temp_svcstatus->time_unknown;
	time_critical=temp_svcstatus->time_critical;

	if(temp_svcstatus->state_type==HARD_STATE){

		if(temp_svcstatus->status==SERVICE_OK || temp_svcstatus->status==SERVICE_RECOVERY)
			time_ok+=time_difference;
		if(temp_svcstatus->status==SERVICE_WARNING)
			time_warning+=time_difference;
		if(temp_svcstatus->status==SERVICE_UNKNOWN)
			time_unknown+=time_difference;
		if(temp_svcstatus->status==SERVICE_CRITICAL || temp_svcstatus->status==SERVICE_UNREACHABLE || temp_svcstatus->status==SERVICE_HOST_DOWN)
			time_critical+=time_difference;
	        }
	else{
		if(temp_svcstatus->last_hard_state==SERVICE_OK || temp_svcstatus->last_hard_state==SERVICE_RECOVERY)
			time_ok+=time_difference;
		if(temp_svcstatus->last_hard_state==SERVICE_WARNING)
			time_warning+=time_difference;
		if(temp_svcstatus->last_hard_state==SERVICE_UNKNOWN)
			time_unknown+=time_difference;
		if(temp_svcstatus->last_hard_state==SERVICE_CRITICAL || temp_svcstatus->last_hard_state==SERVICE_UNREACHABLE || temp_svcstatus->last_hard_state==SERVICE_HOST_DOWN)
			time_critical+=time_difference;
	        }

	total_monitored_time=time_ok+time_warning+time_unknown+time_critical;

	if(total_monitored_time==0L){
		percent_time_ok=0.0;
		percent_time_warning=0.0;
		percent_time_unknown=0.0;
		percent_time_critical=0.0;
	        }
	else{
		percent_time_ok=(float)(((float)time_ok/(float)total_monitored_time)*100.0);
		percent_time_warning=(float)(((float)time_warning/(float)total_monitored_time)*100.0);
		percent_time_unknown=(float)(((float)time_unknown/(float)total_monitored_time)*100.0);
		percent_time_critical=(float)(((float)time_critical/(float)total_monitored_time)*100.0);
	        }

	*tmt=total_monitored_time;
	*tok=time_ok;
	*twarn=time_warning;
	*tun=time_unknown;
	*tcrit=time_critical;
	*ptok=percent_time_ok;
	*ptwarn=percent_time_warning;
	*ptun=percent_time_unknown;
	*ptcrit=percent_time_critical;

	return;
        }






/**********************************************************
 *************** COMMON HTML FUNCTIONS ********************
 **********************************************************/

void display_info_table(char *title,int refresh, authdata *current_authdata){
	time_t current_time;
	char date_time[MAX_DATETIME_LENGTH];
	int result;

	/* read program status */
	result=read_all_status_data(DEFAULT_CGI_CONFIG_FILE,READ_PROGRAM_STATUS);

	printf("<TABLE CLASS='infoBox' BORDER=1 CELLSPACING=0 CELLPADDING=0>\n");
	printf("<TR><TD CLASS='infoBox'>\n");
	printf("<DIV CLASS='infoBoxTitle'>%s</DIV>\n",title);

	time(&current_time);
	get_time_string(&current_time,date_time,(int)sizeof(date_time),LONG_DATE_TIME);

	printf("Last Updated: %s<BR>\n",date_time);
	if(refresh==TRUE)
		printf("Updated every %d seconds<br>\n",refresh_rate);

	printf("Nagios<sup>TM</sup> - <A HREF='http://www.nagios.org' TARGET='_new' CLASS='homepageURL'>www.nagios.org</A><BR>\n");

	if(current_authdata!=NULL)
		printf("Logged in as <i>%s</i><BR>\n",(current_authdata->username==NULL)?"?":current_authdata->username);

	get_nagios_process_info();

	if(nagios_process_state!=STATE_OK)
		printf("<DIV CLASS='infoBoxBadProcStatus'>Warning: Monitoring process may not be running!<BR>Click <A HREF='%s?type=%d'>here</A> for more info.</DIV>",EXTINFO_CGI,DISPLAY_PROCESS_INFO);

	if(result==ERROR)
		printf("<DIV CLASS='infoBoxBadProcStatus'>Warning: Could not read program status information!</DIV>");

	else{
		if(enable_notifications==FALSE)
			printf("<DIV CLASS='infoBoxBadProcStatus'>- Notifications are disabled</DIV>");

		if(execute_service_checks==FALSE)
			printf("<DIV CLASS='infoBoxBadProcStatus'>- Service checks are disabled</DIV>");
	        }

	printf("</TD></TR>\n");
	printf("</TABLE>\n");

	return;
        }



void display_nav_table(char *url,int archive){
	char date_time[MAX_DATETIME_LENGTH];
	char archive_file[MAX_INPUT_BUFFER];

	if(log_rotation_method!=LOG_ROTATION_NONE){
		printf("<table border=0 cellspacing=0 cellpadding=0 CLASS='navBox'>\n");
		printf("<tr>\n");
		printf("<td align=center valign=center CLASS='navBoxItem'>\n");
		if(archive==0){
			printf("Latest Archive<br>");
			printf("<a href='%sarchive=1'><img src='%s%s' border=0 alt='Latest Archive'></a>",url,url_images_path,LEFT_ARROW_ICON);
		        }
		else{
			printf("Earlier Archive<br>");
			printf("<a href='%sarchive=%d'><img src='%s%s' border=0 alt='Earlier Archive'></a>",url,archive+1,url_images_path,LEFT_ARROW_ICON);
		        }
		printf("</td>\n");

		printf("<td width=15></td>\n");

		printf("<td align=center CLASS='navBoxDate'>\n");
		printf("<DIV CLASS='navBoxTitle'>Log File Navigation</DIV>\n");
		get_time_string(&last_scheduled_log_rotation,date_time,(int)sizeof(date_time),LONG_DATE_TIME);
		printf("%s",date_time);
		printf("<br>to<br>");
		if(archive==0)
			printf("Present..");
		else{
			get_time_string(&this_scheduled_log_rotation,date_time,(int)sizeof(date_time),LONG_DATE_TIME);
			printf("%s",date_time);
		        }
		printf("</td>\n");

		printf("<td width=15></td>\n");
		if(archive!=0){

			printf("<td align=center valign=center CLASS='navBoxItem'>\n");
			if(archive==1){
				printf("Current Log<br>");
				printf("<a href='%s'><img src='%s%s' border=0 alt='Current Log'></a>",url,url_images_path,RIGHT_ARROW_ICON);
			        }
			else{
				printf("More Recent Archive<br>");
				printf("<a href='%sarchive=%d'><img src='%s%s' border=0 alt='More Recent Archive'></a>",url,archive-1,url_images_path,RIGHT_ARROW_ICON);
			        }
			printf("</td>\n");
		        }
		else
			printf("<td><img src='%s%s' border=0 width=75 height=1></td>\n",url_images_path,EMPTY_ICON);

		printf("</tr>\n");

		printf("</table>\n");
	        }

	get_log_archive_to_use(archive,archive_file,sizeof(archive_file)-1);
	printf("<BR><DIV CLASS='navBoxFile'>File: %s</DIV>\n",archive_file);

	return;
        }



/* prints the additional notes url for a host (with macros substituted) */
void print_host_notes_url(hostextinfo *temp_hostextinfo){
	char input_buffer[MAX_INPUT_BUFFER]="";
	char output_buffer[MAX_INPUT_BUFFER]="";
	char *temp_buffer;
	int in_macro=FALSE;
	host *temp_host;

	if(temp_hostextinfo==NULL)
		return;

	if(temp_hostextinfo->notes_url==NULL)
		return;

	temp_host=find_host(temp_hostextinfo->host_name,NULL);
	if(temp_host==NULL){
		printf("%s",temp_hostextinfo->notes_url);
		return;
	        }

	strncpy(input_buffer,temp_hostextinfo->notes_url,sizeof(input_buffer)-1);
	output_buffer[sizeof(input_buffer)-1]='\x0';

	for(temp_buffer=my_strtok(input_buffer,"$");temp_buffer!=NULL;temp_buffer=my_strtok(NULL,"$")){

		if(in_macro==FALSE){
			if(strlen(output_buffer)+strlen(temp_buffer)<sizeof(output_buffer)-1){
				strncat(output_buffer,temp_buffer,sizeof(output_buffer)-strlen(output_buffer)-1);
				output_buffer[sizeof(output_buffer)-1]='\x0';
			        }
			in_macro=TRUE;
			}
		else{

			if(strlen(output_buffer)+strlen(temp_buffer) < sizeof(output_buffer)-1){

				if(!strcmp(temp_buffer,"HOSTNAME"))
					strncat(output_buffer,url_encode(temp_host->name),sizeof(output_buffer)-strlen(output_buffer)-1);

				else if(!strcmp(temp_buffer,"HOSTADDRESS"))
					strncat(output_buffer,(temp_host->address==NULL)?"":url_encode(temp_host->address),sizeof(output_buffer)-strlen(output_buffer)-1);
			        }

			in_macro=FALSE;
		        }
	        }

	printf("%s",output_buffer);

	return;
        }



/* prints the additional notes url for a service (with macros substituted) */
void print_service_notes_url(serviceextinfo *temp_serviceextinfo){
	char input_buffer[MAX_INPUT_BUFFER]="";
	char output_buffer[MAX_INPUT_BUFFER]="";
	char *temp_buffer;
	int in_macro=FALSE;
	service *temp_service;
	host *temp_host;

	if(temp_serviceextinfo==NULL)
		return;

	if(temp_serviceextinfo->notes_url==NULL)
		return;

	temp_service=find_service(temp_serviceextinfo->host_name,temp_serviceextinfo->description,NULL);
	if(temp_service==NULL){
		printf("%s",temp_serviceextinfo->notes_url);
		return;
	        }

	temp_host=find_host(temp_serviceextinfo->host_name,NULL);

	strncpy(input_buffer,temp_serviceextinfo->notes_url,sizeof(input_buffer)-1);
	input_buffer[sizeof(input_buffer)-1]='\x0';

	for(temp_buffer=my_strtok(input_buffer,"$");temp_buffer!=NULL;temp_buffer=my_strtok(NULL,"$")){

		if(in_macro==FALSE){
			if(strlen(output_buffer)+strlen(temp_buffer)<sizeof(output_buffer)-1){
				strncat(output_buffer,temp_buffer,sizeof(output_buffer)-strlen(output_buffer)-1);
				output_buffer[sizeof(output_buffer)-1]='\x0';
			        }
			in_macro=TRUE;
			}
		else{

			if(strlen(output_buffer)+strlen(temp_buffer) < sizeof(output_buffer)-1){

				if(!strcmp(temp_buffer,"HOSTNAME"))
					strncat(output_buffer,url_encode(temp_service->host_name),sizeof(output_buffer)-strlen(output_buffer)-1);

				else if(!strcmp(temp_buffer,"HOSTADDRESS") && temp_host!=NULL)
					strncat(output_buffer,(temp_host->address==NULL)?"":url_encode(temp_host->address),sizeof(output_buffer)-strlen(output_buffer)-1);

				else if(!strcmp(temp_buffer,"SERVICEDESC"))
					strncat(output_buffer,url_encode(temp_service->description),sizeof(output_buffer)-strlen(output_buffer)-1);
			        }

			in_macro=FALSE;
		        }
	        }

	printf("%s",output_buffer);

	return;
        }




/* include user-defined SSI footers or headers */
void include_ssi_files(char *cgi_name, int type){
	char common_ssi_file[MAX_INPUT_BUFFER];
	char cgi_ssi_file[MAX_INPUT_BUFFER];
	char raw_cgi_name[MAX_INPUT_BUFFER];
	char *stripped_cgi_name;
	int x;

	/* common header or footer */
	snprintf(common_ssi_file,sizeof(common_ssi_file)-1,"%scommon-%s.ssi",physical_ssi_path,(type==SSI_HEADER)?"header":"footer");
	common_ssi_file[sizeof(common_ssi_file)-1]='\x0';

	/* CGI-specific header or footer */
	strncpy(raw_cgi_name,cgi_name,sizeof(raw_cgi_name)-1);
	raw_cgi_name[sizeof(raw_cgi_name)-1]='\x0';
	stripped_cgi_name=strtok(raw_cgi_name,".");
	snprintf(cgi_ssi_file,sizeof(cgi_ssi_file)-1,"%s%s-%s.ssi",physical_ssi_path,(stripped_cgi_name==NULL)?"":stripped_cgi_name,(type==SSI_HEADER)?"header":"footer");
	cgi_ssi_file[sizeof(cgi_ssi_file)-1]='\x0';
	for(x=0;x<strlen(cgi_ssi_file);x++)
		cgi_ssi_file[x]=tolower(cgi_ssi_file[x]);

	if(type==SSI_HEADER){
		include_ssi_file(common_ssi_file);
		include_ssi_file(cgi_ssi_file);
	        }
	else{
		include_ssi_file(cgi_ssi_file);
		include_ssi_file(common_ssi_file);
	        }

	return;
        }



/* include user-defined SSI footer or header */
void include_ssi_file(char *filename){
	char buffer[MAX_INPUT_BUFFER];
	FILE *fp;

	fp=fopen(filename,"r");
	if(fp==NULL)
		return;

	/* print all lines in the SSI file */
	while(fgets(buffer,(int)(sizeof(buffer)-1),fp)!=NULL)
		printf("%s",buffer);

	fclose(fp);
	
	return;
        }


/* displays an error if CGI config file could not be read */
void cgi_config_file_error(char *config_file){

	printf("<H1>Whoops!</H1>\n");

	printf("<P><STRONG><FONT COLOR='RED'>Error: Could not open CGI config file '%s' for reading!</FONT></STRONG></P>\n",config_file);

	printf("<P>\n");
	printf("Here are some things you should check in order to resolve this error:\n");
	printf("</P>\n");

	printf("<P>\n");
	printf("<OL>\n");

	printf("<LI>Make sure you've installed a CGI config file in its proper location.  See the error message about for details on where the CGI is expecting to find the configuration file.  A sample CGI configuration file (named <b>cgi.cfg</b>) can be found in the <b>sample-config/</b> subdirectory of the Nagios source code distribution.\n");
	printf("<LI>Make sure the user your web server is running as has permission to read the CGI config file.\n");

	printf("</OL>\n");
	printf("</P>\n");

	printf("<P>\n");
	printf("Make sure you read the documentation on installing and configuring Nagios thoroughly before continuing.  If all else fails, try sending a message to one of the mailing lists.  More information can be found at <a href='http://www.nagios.org'>http://www.nagios.org</a>.\n");
	printf("</P>\n");

	return;
        }



/* displays an error if main config file could not be read */
void main_config_file_error(char *config_file){

	printf("<H1>Whoops!</H1>\n");

	printf("<P><STRONG><FONT COLOR='RED'>Error: Could not open main config file '%s' for reading!</FONT></STRONG></P>\n",config_file);

	printf("<P>\n");
	printf("Here are some things you should check in order to resolve this error:\n");
	printf("</P>\n");

	printf("<P>\n");
	printf("<OL>\n");

	printf("<LI>Make sure you've installed a main config file in its proper location.  See the error message about for details on where the CGI is expecting to find the configuration file.  A sample main configuration file (named <b>nagios.cfg</b>) can be found in the <b>sample-config/</b> subdirectory of the Nagios source code distribution.\n");
	printf("<LI>Make sure the user your web server is running as has permission to read the main config file.\n");

	printf("</OL>\n");
	printf("</P>\n");

	printf("<P>\n");
	printf("Make sure you read the documentation on installing and configuring Nagios thoroughly before continuing.  If all else fails, try sending a message to one of the mailing lists.  More information can be found at <a href='http://www.nagios.org'>http://www.nagios.org</a>.\n");
	printf("</P>\n");

	return;
        }


/* displays an error if object data could not be read */
void object_data_error(void){

	printf("<H1>Whoops!</H1>\n");

	printf("<P><STRONG><FONT COLOR='RED'>Error: Could not read object configuration data!</FONT></STRONG></P>\n");

	printf("<P>\n");
	printf("Here are some things you should check in order to resolve this error:\n");
	printf("</P>\n");

	printf("<P>\n");
	printf("<OL>\n");

	printf("<LI>Verify configuration options using the <b>-v</b> command-line option to check for errors.\n");
	printf("<LI>Check the Nagios log file for messages relating to startup or status data errors.\n");
	printf("<LI>Make sure you've compiled the main program and the CGIs to use the same object data storage options (i.e. default text file or template-based file).\n");

	printf("</OL>\n");
	printf("</P>\n");

	printf("<P>\n");
	printf("Make sure you read the documentation on installing, configuring and running Nagios thoroughly before continuing.  If all else fails, try sending a message to one of the mailing lists.  More information can be found at <a href='http://www.nagios.org'>http://www.nagios.org</a>.\n");
	printf("</P>\n");

	return;
        }


/* displays an error if status data could not be read */
void status_data_error(void){

	printf("<H1>Whoops!</H1>\n");

	printf("<P><STRONG><FONT COLOR='RED'>Error: Could not read host and service status information!</FONT></STRONG></P>\n");

	printf("<P>\n");
	printf("The most common cause of this error message (especially for new users), is the fact that Nagios is not actually running.  If Nagios is indeed not running, this is a normal error message.  It simply indicates that the CGIs could not obtain the current status of hosts and services that are being monitored.  If you've just installed things, make sure you read the documentation on starting Nagios.\n");
	printf("</P>\n");

	printf("<P>\n");
	printf("Some other things you should check in order to resolve this error include:\n");
	printf("</P>\n");

	printf("<P>\n");
	printf("<OL>\n");

	printf("<LI>Check the Nagios log file for messages relating to startup or status data errors.\n");
	printf("<LI>Always verify configuration options using the <b>-v</b> command-line option before starting or restarting Nagios!\n");
	printf("<LI>Make sure you've compiled the main program and the CGIs to use the same status data storage options (i.e. text file or database).  If the main program is storing status data in a text file and the CGIs are trying to read status data from a database, you'll have problems.\n");

	printf("</OL>\n");
	printf("</P>\n");

	printf("<P>\n");
	printf("Make sure you read the documentation on installing, configuring and running Nagios thoroughly before continuing.  If all else fails, try sending a message to one of the mailing lists.  More information can be found at <a href='http://www.nagios.org'>http://www.nagios.org</a>.\n");
	printf("</P>\n");

	return;
        }




#ifdef CONTEXT_HELP

/* displays context-sensitive help window */
void display_context_help(char *chid){
	char *icon=CONTEXT_HELP_ICON1;

	/* change icon if necessary */
	if(!strcmp(chid,CONTEXTHELP_TAC))
		icon=CONTEXT_HELP_ICON2;

	printf("<a href='%s%s.html' target='cshw' onClick='javascript:window.open(\"%s%s.html\",\"cshw\",\"width=550,height=600,toolbar=0,location=0,status=0,resizable=1,scrollbars=1\");return true'><img src='%s%s' border=0 alt='Display context-sensitive help for this screen'></a>\n",url_context_help_path,chid,url_context_help_path,chid,url_images_path,icon);

	return;
        }

#endif