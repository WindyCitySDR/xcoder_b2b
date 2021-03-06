#include "xcoder_b2b.h"
#include "xcoder_b2b_load.h"

/********************************************************************************
 *
 * FUNCTION PROTOTYPES
 *
 *********************************************************************************/

static int
mod_init(void);
static int
parse_invite(struct sip_msg *msg);
static int
parse_inDialog_invite(struct sip_msg *msg);
static int
parse_200OK(struct sip_msg *msg);
static int
parse_ACK(struct sip_msg* msg);
static int
parse_cancel(struct sip_msg* msg);
static int
parse_bye(struct sip_msg* msg);
static int
general_failure(struct sip_msg *msg);
static int
parse_refer(struct sip_msg *msg);
static int
parse_183(struct sip_msg *msg);
static void
mod_destroy(void);
static int
check_overtime_conns(void);
int
create_call(conn * connection, client * caller);
int
get_socket(void);
int
send_remove_to_xcoder(conn * connection);

b2bl_api_t b2b_logic_load; //To load b2b_logic functions
b2b_api_t b2b_api; //To load b2b_entitities functions

static struct mi_root* mi_xcoder_b2b_update_codecs(struct mi_root* cmd, void* param);

/** MI commands */
static mi_export_t mi_cmds[] = {
	{ "xcoder_b2b_update_codecs", mi_xcoder_b2b_update_codecs, 0,  0,  0},
	{0,		0,		0,	0,	0}
};

static cmd_export_t cmds[] =
{
{ "parse_invite", (cmd_function) parse_invite, 0, 0, 0, REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE | BRANCH_ROUTE | LOCAL_ROUTE },
{ "parse_inDialog_invite", (cmd_function) parse_inDialog_invite, 0, 0, 0, REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE | BRANCH_ROUTE | LOCAL_ROUTE },
{ "parse_200OK", (cmd_function) parse_200OK, 0, 0, 0, REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE | BRANCH_ROUTE | LOCAL_ROUTE },
{ "parse_ACK", (cmd_function) parse_ACK, 0, 0, 0, REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE | BRANCH_ROUTE | LOCAL_ROUTE },
{ "parse_cancel", (cmd_function) parse_cancel, 0, 0, 0, REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE | BRANCH_ROUTE | LOCAL_ROUTE },
{ "parse_bye", (cmd_function) parse_bye, 0, 0, 0, REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE | BRANCH_ROUTE | LOCAL_ROUTE },
{ "general_failure", (cmd_function) general_failure, 0, 0, 0, REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE | BRANCH_ROUTE | LOCAL_ROUTE },
{ "parse_refer", (cmd_function) parse_refer, 0, 0, 0, REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE | BRANCH_ROUTE | LOCAL_ROUTE },
{ "parse_183", (cmd_function) parse_183, 0, 0, 0, REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE | BRANCH_ROUTE | LOCAL_ROUTE },
{ "check_overtime_conns", (cmd_function) check_overtime_conns, 0, 0, 0, REQUEST_ROUTE | ONREPLY_ROUTE | FAILURE_ROUTE | BRANCH_ROUTE | LOCAL_ROUTE | TIMER_ROUTE },
{ "load_xcoder", (cmd_function) load_xcoder, 0, 0, 0, 0 },
{ 0, 0, 0, 0, 0, 0 } };

static param_export_t params[] =
{
{ "conf_file", STR_PARAM, &conf_file },
{ 0, 0, 0 } };

struct module_exports exports=
{
   "xcoder_b2b", /* module name*/
   MODULE_VERSION,
   DEFAULT_DLFLAGS, /* dlopen flags */
   cmds, /* exported functions */
   params, /* module parameters */
   0, /* exported statistics */
   mi_cmds, /* exported MI functions */
   0, /* exported pseudo-variables */
   0, /* extra processes */
   mod_init, /* module initialization function */
   (response_function) 0, /* response function */
   (destroy_function) mod_destroy, /* destroy function */
   0, /* per-child init function */
};

/******************************************************************************
 *        NAME: load_xcoder
 * DESCRIPTION: Function that bind function 'add_b2b_callID' to be used by other modules.
 *
 *              xcoder :    structure to be filled with function names to be binded
 *****************************************************************************/

int
load_xcoder(struct xcoder_binds *xcoder)
{
   xcoder->add_b2b_callID = add_b2b_callID;
   xcoder->free_xcoder_resources = free_xcoder_resources;
   return 1;
}

/******************************************************************************
 *        NAME: clean_client
 * DESCRIPTION: Receives a client structure and cleans the structure.
 *
 *              cli :    structure to be cleaned
 *****************************************************************************/

int
clean_client(client * cli)
{
	LM_INFO("Cleaning client. client_id=%d | client_tag=%s | client_state=%d\n",cli->id,cli->tag,cli->s);

	bzero(cli->src_ip, 25);
	bzero(cli->dst_ip, 25);
	bzero(cli->user_name, 128);
	bzero(cli->user_agent, 128);
	bzero(cli->conn_ip, 25);
	bzero(cli->tag, 128);
	bzero(cli->b2b_tag, 128);
	bzero(cli->original_port, 7);
	bzero(cli->media_type, 10);
	bzero(cli->dst_audio, 7);
	bzero(cli->dst_video, 7);
	bzero(cli->payload_str, 50);
	cli->is_empty = 0;
	cli->s = EMPTY;

	int l = 0;
	for (l = 0; l < MAX_PAYLOADS; l++)
	{
	  bzero(cli->payloads[l].payload, 32);
	  bzero(cli->payloads[l].attr_rtpmap_line, 100);
	  bzero(cli->payloads[l].attr_fmtp_line, 100);
	  cli->payloads[l].is_empty = 0;
	}
	return OK;
}



/******************************************************************************
 *        NAME: clean_connection
 * DESCRIPTION: Receives a connection structure and cleans the structure.
 *
 *              connection :    structure to be cleaned
 *****************************************************************************/

int
clean_connection(conn * connection)
{
   LM_INFO("Cleaning connection. b2bcallid=%d | call_id=%s | b2b_gen_call-id %s\n",
         connection->id, connection->call_id, connection->b2b_client_callID);
   LM_NOTICE("Terminating call. b2bcallid=%d",connection->id);
   connection->id = -1;
   connection->xcoder_id = -1;
   connection->conn_time = 0;
   bzero(connection->call_id, 128);
   bzero(connection->b2b_client_callID, 128);
   bzero(connection->b2b_client_serverID, 128);
   bzero(connection->b2b_key, 128);
   bzero(connection->cseq, 64);

   int i = 0;
   for (i = 0; i < MAX_CLIENTS; i++)
   {
      connection->clients[i].id = '\0';
      bzero(connection->clients[i].src_ip, 25);
      bzero(connection->clients[i].dst_ip, 25);
      bzero(connection->clients[i].user_name, 128);
      bzero(connection->clients[i].user_agent, 128);
      bzero(connection->clients[i].conn_ip, 25);
      bzero(connection->clients[i].tag, 128);
      bzero(connection->clients[i].b2b_tag, 128);
      bzero(connection->clients[i].original_port, 7);
      bzero(connection->clients[i].media_type, 10);
      bzero(connection->clients[i].dst_audio, 7);
      bzero(connection->clients[i].dst_video, 7);
      bzero(connection->clients[i].payload_str, 50);
      connection->clients[i].is_empty = 0;
      connection->clients[i].s = TERMINATED;

      int l = 0;
      for (l = 0; l < MAX_PAYLOADS; l++)
      {
         bzero(connection->clients[i].payloads[l].payload, 32);
         bzero(connection->clients[i].payloads[l].attr_rtpmap_line, 100);
         bzero(connection->clients[i].payloads[l].attr_fmtp_line, 100);
         connection->clients[i].payloads[l].is_empty = 0;
      }
   }
   connection->s = TERMINATED;
   return OK;
}

/******************************************************************************
 *        NAME: clean_connection_v2
 * DESCRIPTION: Receives a sip message, find the connection, send remove to xcoder and clean the structures referring to this connection.
 *
 *              msg :    sip message received
 *****************************************************************************/

int
clean_connection_v2(struct sip_msg *msg)
{
   int i = 0;
   char callID[128];
   bzero(callID, 128);
   snprintf(callID, (msg->callid->body.len + 1), "%s", msg->callid->body.s); //Get callID

   for (i = 0; i < MAX_CONNECTIONS; i++)
   {
      if (connections[i].s != EMPTY && connections[i].s != TERMINATED && (strcmp(connections[i].call_id, callID) == 0))
      {
    	 LM_INFO("Found connection to clean. b2bcallid=%d | call_id=%s | b2b_gen_call_id %s\n",connections[i].id,connections[i].call_id,connections[i].b2b_client_callID);
    	 send_remove_to_xcoder(&(connections[i]));
         clean_connection(&(connections[i]));
         return OK;
      }
   }
   return GENERAL_ERROR;
}

/******************************************************************************
 *        NAME: get_and_increment
 * DESCRIPTION: Return a value stores in a count variable and increments it.
 * 				This is a synchronized operation.
 *
 *		count :	   count variable to be incremented
 *              value :    value to store the current count
 *		
 *****************************************************************************/

int
get_and_increment(int * count, int * value)
{
   lock_get(conn_lock);

   *value = *count;
   *count = *count + 1;
   lock_release(conn_lock);
   if (*count >= (INT_MAX - 2))
   {
      *count = 0;
   }
   return OK;
}

/******************************************************************************
 *        NAME: check_connections
 * DESCRIPTION: This function prints the number of active connections and clients.
 *
 *              connection :    Function that invoke 'check_connections'
 *****************************************************************************/

int
check_connections(void)
{
   int active_conn = 0;
   int active_cli = 0;
   time_t current_time;
   current_time = time(0);
   int i = 0;
   for (i = 0; i < MAX_CONNECTIONS; i++)
   {
      if (connections[i].s != TERMINATED && connections[i].s != EMPTY)
      {
         active_conn++;

         int k = 0;
         for (k = 0; k < MAX_CLIENTS; k++)
         {
            if (connections[i].clients[k].is_empty == 1)
            {
               active_cli++;
            }
         }
      }
   }
   LM_INFO("There are %d active connections and %d active clients\n",active_conn,active_cli);

   return OK;
}

/******************************************************************************
 *        NAME: cancel_connection
 * DESCRIPTION: Terminates a connection
 *
 *              connection :    connection to terminate
 *****************************************************************************/

int
cancel_connection(conn * connection)
{
   LM_NOTICE("Cancelling connection. b2bcallid=%d | call_id=%s | conn_state=%d\n", connection->id,connection->call_id,connection->s);
   str key_b2b;
   if (connection == NULL)
   {
      LM_ERR("ERROR: Connection to cancel is null. error_code=%d\n",PARSER_ERROR);
      return PARSER_ERROR;
   }
   key_b2b.s = connection->b2b_key;
   key_b2b.len = strlen(connection->b2b_key);
   b2b_logic_load.terminate_call(&(key_b2b));

   clean_connection(connection);
   return OK;
}

/******************************************************************************
 *        NAME: get_client
 * DESCRIPTION: Receives a client and returns the destination client. Only work for 2 clients per connection
 *
 *              connection :	connection containing the clients
 *              src_cli :       source client
 *				dst_cli :	destination client returned
 *****************************************************************************/

int
get_client(conn * connection, client * src_cli, client ** dst_cli)
{
//        lock_get(conn_lock);

   int i = 0;
//	char * dst_ip = src_cli->dst_ip;
   LM_INFO("Source client. b2bcallid=%d | conn_state=%d | client_id=%d | src_ip=%s | dst_ip=%s | tag=%s | call_id=%s\n",
         connection->id, connection->s, src_cli->id, src_cli->src_ip, src_cli->dst_ip, src_cli->tag, connection->call_id);

   for (i = 0; i < MAX_CLIENTS; i++)
   {
      LM_INFO("To compare. Client number %d\n",i);

      if (connection->s != TERMINATED && connection->s != EMPTY && connection->clients[i].is_empty != 0
            && connection->clients[i].id != src_cli->id)
      {
         *dst_cli = &(connection->clients[i]);
         break;
      }
   }
   /*	LM_INFO("Source client. Conn id %d | Conn_state %d | Id %d | Ip %s | dst_ip %s | tag=%s | call-id %s\n",
    connection->id,connection->s,src_cli->id,src_cli->src_ip,src_cli->dst_ip,src_cli->tag,connection->call_id);
    for(i=0;i<MAX_CLIENTS;i++)
    {
    LM_INFO("To compare. Id %d | IP %s | Dst_Ip %s | tag=%s | conn ip %s | b2b_tag=%s | dst_audio %s\n",
    connection->clients[i].id,connection->clients[i].src_ip,connection->clients[i].dst_ip,connection->clients[i].tag,
    connection->clients[i].conn_ip,connection->clients[i].b2b_tag,connection->clients[i].dst_audio);

    if(connection->s!=TERMINATED && connection->s!=EMPTY && ( strcmp(connection->clients[i].src_ip,dst_ip)==0 ) && (strcmp(connection->clients[i].tag,src_cli->tag)!=0))
    {
    LM_NOTICE("Encountered destination client. Id = %d\n",connection->clients[i].id);
    *dst_cli=&(connection->clients[i]);
    break;
    }
    }*/
   if (i == MAX_CLIENTS)
   {
      LM_ERR("ERROR: No client encountered.b2bcallid=%d | all_id=%s | conn_state=%d | client_id=%d | client_state=%d | error_code=%d\n",
    		  connection->id,connection->call_id,connection->s,src_cli->id,src_cli->s,GENERAL_ERROR);
      return GENERAL_ERROR;
   }
   //      lock_release(conn_lock);

   return OK;
}

/******************************************************************************
 *        NAME: get_active_payload
 * DESCRIPTION: Receives a client as argument and returns the active payload for this client
 *
 *              cli : 			client to retrieve payload
 *              chosen :        payload returned
 *****************************************************************************/

int
get_active_payload(client * cli, char * chosen)
{
   int i = 0;
   for (i = 0; i < MAX_PAYLOADS; i++)
   {
      if (cli->payloads[i].is_empty == 1)
      {
         sprintf(chosen, cli->payloads[i].payload);
         return OK;
      }
   }
   LM_ERR("No payload found. client_id=%d | client state=%d | src_ip=%s | error_code=%d\n",
		   cli->id,cli->s,cli->src_ip,NOT_ACCEPTABLE_HERE);
   return NOT_ACCEPTABLE_HERE;
}

/******************************************************************************
 *        NAME: count_length_to_end
 * DESCRIPTION: Count the number of characters to end (\n,\r,\0)
 *
 *              sdp :           string to read
 *              i :             position to star to read
 *****************************************************************************/

int
count_length_to_end(char * sdp, int i)
{
   int len = 0;
   int pos = i;
   while (sdp[pos] != '\0' && sdp[pos] != '\n' && sdp[pos] != '\r')
   {
      len++;
      pos++;
   }
   return len;
}

/******************************************************************************
 *        NAME: read_until_end
 * DESCRIPTION: Reads characters until end of line
 *
 *              sdp :           string to read
 *              i :             position to star to read
 *              to_fill :       string representing the characters readed
 *****************************************************************************/

int
read_until_end(char * sdp, int * i, char * to_fill)
{
   int index = 0;
   int pos = *i;
   while (sdp[pos] != '\0' && sdp[pos] != '\n' && sdp[pos] != '\r')
   {
      to_fill[index] = sdp[pos];
      index++;
      pos++;
   }

   if (pos == *i)
   {
      //LM_WARN("Warning: no character readed.\n");
      return GENERAL_ERROR;
   }

   *i = pos;
   return OK;
}

/******************************************************************************
 *        NAME: read_until_char
 * DESCRIPTION: Reads characters until a pattern received as arguments is found
 *
 *              sdp :           string to read
 *              i :             position to star to read
 *		pattern : 	pattern to match
 *		to_fill :	string representing the characters readed
 *****************************************************************************/

int
read_until_char(char * sdp, int * i, const char pattern, char * to_fill)
{
   int index = 0;
   int pos = *i;
   while (sdp[pos] != '\0' && sdp[pos] != '\n' && sdp[pos] != '\r' && sdp[pos] != pattern)
   {
      to_fill[index] = sdp[pos];
      index++;
      pos++;
   }

   if (pos == *i)
   {
      LM_ERR("ERROR: no character readed. error_code=%d\n",GENERAL_ERROR);
      return GENERAL_ERROR;
   }

   if (sdp[pos] != pattern)
   {
      //LM_WARN("WARNING: Pattern not found.\n");
      *to_fill = '\0';
   }
   *i = pos;

   return OK;
}

/******************************************************************************
 *        NAME: move_to_end
 * DESCRIPTION: Move pointer to end of line
 *
 *              sdp :           string to read
 *              i :             pointer to move
 *****************************************************************************/

int
move_to_end(char * sdp, int * i)
{
   int pos = *i;
   while (sdp[pos] != '\n' && sdp[pos] != '\0')
   {
      pos++;
   }
   if (sdp[pos] == '\n')
      pos++;

   if (*i == pos)
   {
      LM_INFO("WARNING: Pointer returned is the same as received.\n");
      return GENERAL_ERROR;
   }
   *i = pos;

   return OK;
}

/******************************************************************************
 *        NAME: get_word
 * DESCRIPTION: Read the characters from the position received (i) until the next token
 *
 *              sdp :           string to read
 *              i :             position to start read
 *		word :		word returned
 *****************************************************************************/

int
get_word(char * sdp, int * i, char * word)
{
   int index = 0;
   int pos = *i;
   while (sdp[pos] != '\0' && sdp[pos] != '\n' && sdp[pos] != ' ' && sdp[pos] != '\r')
      word[index++] = sdp[pos++];

   if (word == NULL)
   {
      LM_ERR("ERROR: Failed to retrieve word. Str position %d | error_code=%d\n",(*i),GENERAL_ERROR);
      return GENERAL_ERROR;
   }

   *i = pos;
   return OK;
}

/******************************************************************************
 *        NAME: count_lenght_to_next_token
 * DESCRIPTION: This function count number of characters to the next token (\n,\r,\0,' ')
 *
 *              sdp :		string to read
 *              i :		number returned representing the count
 *****************************************************************************/

int
count_lenght_to_next_token(char * sdp, int i)
{
   int len = 0;
   int pos = i;

   while (sdp[pos] != '\0' && sdp[pos] != '\n' && sdp[pos] != '\r' && sdp[pos] != ' ')
   {
      len++;
      pos++;
   }

   return len;
}

/******************************************************************************
 *        NAME: get_value_from_str
 * DESCRIPTION: Retrieve the value of a variable name from a string.
 * 				Format of a string line is : $var_name=$value\r\n
 *
 *              var_name : variable name to retrieve the value
 *              buffer : string to read
 *              value : variable that will hold the value
 *****************************************************************************/

int get_value_from_str(char * var_name, char * buffer, char * value)
{
	int status=OK;
	int i=0;
	char response[64];
	bzero(response,64);

	while (buffer[i] != '\0')
	{
	  status = read_until_char(buffer, &i, '=', response); //Read word until character '='
	  if (strcmp(response, var_name) == 0)
	  {
		 i++; //Increment counter to advance character '='
		 bzero(response, 64);

		 read_until_end(buffer, &i, value); // Read until end of line
	  }
      bzero(response, 64); // Clean response
      move_to_end(buffer, &i); //Move to newline
	}

	if(value[0]=='\0')
	{
		LM_NOTICE("No value retrieved\n");
		return PARSER_ERROR;
	}

	return status;
}

/******************************************************************************
 *        NAME: insert_payloads
 * DESCRIPTION: This function receives a list of payloads, parse this list
 +		and insert each one in the client structure.
 *
 *              connection :	connection containing the client
 *              cli :		client structure to insert payloads
 *		payload_str :	list of payloads
 *****************************************************************************/

int
insert_payloads(conn * connection, client * cli, char * payload_str)
{
   int pos = 0;
   char tmp_payload[32];
   bzero(tmp_payload, 32);

   LM_INFO("Insert playloads. b2bcallid=%d | conn_state=%d\n", connection->id,connection->s);
   sprintf(cli->payload_str, payload_str);

   while (payload_str[pos] != '\0' && payload_str[pos] != '\n' && payload_str[pos] != '\r')
   {
      get_word(payload_str, &pos, tmp_payload);
      int i = 0;
      for (i = 0; i < MAX_PAYLOADS; i++)
      {
         if (cli->payloads[i].is_empty == 0)
         {
            cli->payloads[i].is_empty = 1;
            sprintf(cli->payloads[i].payload, tmp_payload);
            LM_INFO("Inserted payload %s\n", cli->payloads[i].payload);
            break;
         }
      }
      bzero(tmp_payload, 32);
      if (payload_str[pos] != '\0')
         pos++;
   }

   return OK;
}

/******************************************************************************
 *        NAME: str_toUpper
 * DESCRIPTION: This function put a string in Uppercase
 *****************************************************************************/

int
str_toUpper(char * str)
{
   LM_INFO("String to put in uppercase is %s\n", str);
   int i = 0;
   for (i = 0; str[i]; i++)
   {
      str[i] = toupper(str[i]);
   }
   LM_INFO("Upper string : %s\n", str);
   return OK;
}


/******************************************************************************
 *        NAME: remove_newline_str
 * DESCRIPTION: This function removes \r and \n characters from a string
 *
 *		str : string to treat
 *****************************************************************************/

int remove_newline_str(char * str)
{
	int status=OK;
	int index=0;
	while(str[index]!='\0')
	{
		   if(str[index]=='\r' || str[index]=='\n')
			   str[index]=' ';
		   index++;
	}

	return status;
}



/******************************************************************************
 *        NAME: match_payload
 * DESCRIPTION: This function match the codecs that a client supports with codecs that a media relay support.
 *		Match the codecs and if successful returns the payload of chosen codec.
 *
 *		connection :	 connection strucure
 *      cli :            client to match codecs
 *		chosen_payload : matched codec payload
 *****************************************************************************/

int
match_payload(conn * connection, client * cli, char * chosen_payload)
{

   int j = 0;
   LM_INFO("Client payload struct\n");
   for (j = 0; j < MAX_PAYLOADS; j++)
   {
      if (cli->payloads[j].is_empty == 1)
      {
         //If codec name is not present in attribute line, use default name from media relay supported codecs
         if ((strlen(cli->payloads[j].codec) < 1))
         {
            LM_INFO("Codec name not present for payload [%s]. Assigning a default name.\n", cli->payloads[j].payload);

            int m = 0;
            for (m = 0; m < MAX_PAYLOADS; m++)
            {
               char payload[32];
               bzero(payload, 32);
               sprintf(payload, "%d", codecs[m].payload); // Convert to string

               if ((strcmp(cli->payloads[j].payload, payload) == 0)) //Match payload
               {
                  sprintf(cli->payloads[j].codec, "%s", codecs[m].sdpname);
                  LM_INFO("Assigned codec name [%s] for payload [%s]\n", cli->payloads[j].codec, cli->payloads[j].payload);
               }
            }
         }
         str_toUpper(cli->payloads[j].codec); // Put codec in uppercase
         //Inserted exception for g729a, see issue WMS-731
         if((strcmp(cli->payloads[j].codec, "G729A") == 0))
         {
            LM_INFO("Detected sdp codec name 'g729a', changing to g729\n");
            sprintf(cli->payloads[j].codec, "%s", "G729");
         }
         LM_INFO("Pos [%d]. Codec [%s] | Payload [%s]\n", j, cli->payloads[j].codec, cli->payloads[j].payload);
      }
   }

   LM_INFO("Surfmotion payload struct\n");
   for (j = 0; j < MAX_PAYLOADS; j++)
   {
      if (codecs[j].is_empty == 1)
      {
         str_toUpper(codecs[j].name); // Put codec in uppercase
         str_toUpper(codecs[j].sdpname); // Put codec in uppercase
         LM_INFO("Pos [%d]. Codec [%s] | Payload [%d]\n", j, codecs[j].sdpname, codecs[j].payload);
      }
   }

   char codec_client_toUpper[64];
   char codec_surf_toUpper[64];
   bzero(codec_client_toUpper, 64);
   bzero(codec_surf_toUpper, 64);

   int i = 0;
   int index_chosen_codec = 0;
   for (i = 0; i < MAX_PAYLOADS; i++)
   {
      if (cli->payloads[i].is_empty == 1) //Loop throw the client payload list to find a payload
      {
         LM_INFO("Client codec [%s] | payload [%s]\n", cli->payloads[i].codec, cli->payloads[i].payload);
         int l = 0;
         for (l = 0; l < MAX_PAYLOADS; l++) // Loop to the sufmotion payload to try and match a client payload.
         {
            if (codecs[l].is_empty != 1)
               continue; // Advance cyle

            LM_INFO("Compare. cli codec [%s] | surf codec [%s[\n", cli->payloads[i].codec, codecs[l].sdpname);

            if (codecs[l].is_empty == 1 && (strcmp(cli->payloads[i].codec, codecs[l].sdpname) == 0)
                  && (strcmp(cli->payloads[i].codec, "TELEPHONE-EVENT") != 0))
            {
               /// Assign the media relay codec name to client codec name
               sprintf(cli->payloads[i].codec, "%s", codecs[l].name);

               sprintf(chosen_payload, cli->payloads[i].payload);
               LM_NOTICE("Found supported codec: codec=%s | payload=%s | b2bcallid=%d | client_id=%d\n", cli->payloads[i].codec,chosen_payload,connection->id,cli->id);

               bzero(cli->payload_str, 50);
               sprintf(cli->payload_str, "%s", chosen_payload);
               index_chosen_codec = i;

               int k = 0;
               for (k = 0; k < MAX_PAYLOADS; k++)
               {
                  if (cli->payloads[k].is_empty == 1 && k != i) // Eliminate all payloads except chosen payload and dtmf payload (101)
                  {
                     if ((strcmp(cli->payloads[k].codec, "TELEPHONE-EVENT") == 0)) //Check if is a dtmf payload
                     {
                    	LM_INFO("Cleaning codec not matched. codec [%s] | payload [%s]\n", cli->payloads[k].codec, cli->payloads[k].payload);
                        char payload_str_tmp[50]; //Used to aid in sprintf function
                        bzero(payload_str_tmp, 50);
                        sprintf(payload_str_tmp, cli->payload_str); // Copy client payload list ti payload_str_tmp
                        sprintf(cli->payload_str, "%s %s", payload_str_tmp, cli->payloads[k].payload);
                        continue;
                     }
                     cli->payloads[k].is_empty = 0;
                     bzero(cli->payloads[k].attr_rtpmap_line, 100);
                     bzero(cli->payloads[k].attr_fmtp_line, 100);
                  }
               }
               return OK;
            }
         }
      }
   }
   LM_ERR("ERROR, No codec found. client_id=%d | client state=%d | src_ip=%s | tag=%s | error_code=%d\n",
		   cli->id,cli->s,cli->src_ip,cli->tag,NOT_ACCEPTABLE_HERE);
   return NOT_ACCEPTABLE_HERE;
}

/******************************************************************************
 *        NAME: get_all_suported_att
 * DESCRIPTION: Retrieves all supported attributes and stores them in a string.
 *****************************************************************************/

int
get_all_supported_att(char * att)
{
   int status = OK;
   char att_tmp[XCODER_MAX_MSG_SIZE];
   int i = 0;

   for (i = 0; i < MAX_PAYLOADS; i++)
   {
      if (codecs[i].is_empty == 1)
      {
         bzero(att_tmp, XCODER_MAX_MSG_SIZE);
         sprintf(att_tmp, att);

         if (codecs[i].frequency != -1)
         {
            sprintf(att, "%s%s", att_tmp, codecs[i].attr_rtpmap_line);
            bzero(att_tmp, XCODER_MAX_MSG_SIZE);
            sprintf(att_tmp, att);
         }

         if ((strlen(codecs[i].fmtp)) > 0)
         {
            sprintf(att, "%s%s", att_tmp, codecs[i].attr_fmtp_line);
         }
      }
   }
   //Remove \r and \n from attribute copy message

   char att_copy[strlen(att)+2];
   int size=sizeof(att_copy)/sizeof(char);
   if( size >= (strlen(att)+2) )
   {
	   snprintf(att_copy,strlen(att)+1,att);
	   remove_newline_str(att_copy);
	   LM_INFO("Supported attribute list : [%s]\n", att_copy);
   }
   else
	   LM_WARN("Failed to allocate space for 'att_copy' variable\n");

   return status;
}

/******************************************************************************
 *        NAME: get_all_suported_payloads
 * DESCRIPTION: Retrives all supported payloads an stores them in a single string.
 *				This string will be included in future sdp message.
 *
 *		payloads : Variable that will hold the list of supported payloads
 *****************************************************************************/

int
get_all_suported_payloads(char * payloads)
{
   int status = OK;
   char payloads_tmp[128];
   int i = 0;

   for (i = 0; i < MAX_PAYLOADS; i++)
   {
      if (codecs[i].is_empty == 1)
      {
         bzero(payloads_tmp, 128);
         sprintf(payloads_tmp, payloads);
         sprintf(payloads, "%s%d ", payloads_tmp, codecs[i].payload);
      }
   }
   LM_INFO("Supported payload list=[%s]\n", payloads);

   return status;
}

/******************************************************************************
 *        NAME: insert_call_type
 * DESCRIPTION: This function insert call type received as argument in the client structure.
 *
 * 		connection : Connection that the client belongs
 * 		cli : Client to insert call type.
 * 		call_type : Variable that holds type of call.
 *****************************************************************************/

int
insert_call_type(conn * connection, client * cli, char * call_type)
{
   sprintf(cli->media_type, call_type);
   return OK;
}

/******************************************************************************
 *        NAME: put_attribute
 * DESCRIPTION: This function will insert a codec attribute int the payload_struct structure.
 *		This function read the attribute line, match the type received as an argument
 *		and insert it in the structure.
 *
 *     	sdp :		string representing the sdp message
 *		i :     	position to begin the extraction of the attribute line.
 * 		type :		type of attribute ("rtpmap" or "fmtp")
 *		payload :	payload value
 *		codec :		Codec name
 *		payloads :	payload structure to insert the attribute line
 *****************************************************************************/

int
put_attribute(char * sdp, int * i, char * type, char * payload, char * codec, payload_struct * payloads)
{
   char line[256];
   bzero(line, 256);
   read_until_end(sdp, i, line); //Retrieve attribute line

   int j = 0;
   for (j = 0; j < MAX_PAYLOADS; j++)
   {
      LM_INFO("Compare %s with %s. is_empty=%d\n", payloads[j].payload, payload,payloads[j].is_empty);
      if (payloads[j].is_empty == 1 && strcmp(payloads[j].payload, payload) == 0) //Match for list of payloads received
      {
         if (strcmp(type, "rtpmap") == 0)
         {
            sprintf(payloads[j].attr_rtpmap_line, "%s\r\n", line);
            sprintf(payloads[j].codec, codec); // Copy codec value to payloads structure. Codec name is in "rtpmap" line
            LM_INFO("Type=%s | inserted codec [%s] with payload [%s]\n", type, payloads[j].codec, payload);
            break;
         }
         if ((strcmp(type, "fmtp") == 0))
         {
            LM_INFO("Type=%s. Inserted fmtp line\n", type);
            sprintf(payloads[j].attr_fmtp_line, "%s\r\n", line);
         }

         ////////////////////// Check for active payloads with the same name ///////////////////

         int k = 0;
         for (k = (j + 1); k < MAX_PAYLOADS; k++)
         {
            if (payloads[k].is_empty == 1 && strcmp(payloads[k].payload, payload) == 0)
            {
               LM_ERR("ERROR: Found the same payload more than once. Payload=%s | type=%s | error_code=%d\n",payload,type,GENERAL_ERROR);
               return GENERAL_ERROR;
               break;
            }
         }
         break;
      }

   }
   if (j == MAX_PAYLOADS)
   {
      LM_INFO("WARNING.Failed to match payload %s for attribute line : %s\n", payload, line);
      LM_INFO("WARNING.Could be video payloads not yet supported by this version\n");
   }

   return OK;
}

/******************************************************************************
 *        NAME: insert_after_b2b
 * DESCRIPTION: This function will insert characters in the sdp content after a lump received as argument.
 *
 *              msg :           sip message to manipulate.
 *              to_insert :     string to insert
 *		l :		lump that act as a position to insert data.
 *
 *****************************************************************************/

int
insert_after_b2b(struct sip_msg *msg, char * to_insert, struct lump* l)
{
   char * s;
   int len = strlen(to_insert);
   s = pkg_malloc(len);
   if (s == 0)
   {
      LM_ERR("memory allocation failure\n");
      return PARSER_ERROR;
   }

   memcpy(s, to_insert, len);
   if (insert_new_lump_after(l, s, len, 0) == 0)
   {
      LM_ERR("could not insert new lump. String to insert [%s] | error_code=%d\n",to_insert,PARSER_ERROR);
      return PARSER_ERROR;
   }

   return OK;
}

/******************************************************************************
 *        NAME: insert_b2b
 * DESCRIPTION: This function will insert characters in the sdp content
 *
 *              msg :           sip message to manipulate.
 *              position :      position to insert data
 *              to_insert :	string to insert
 *
 *****************************************************************************/

int
insert_b2b(struct sip_msg *msg, char * position, char * to_insert)
{
   struct lump * l;
   char * body;
   body = get_body(msg);

   if ((l = anchor_lump(msg, position - msg->buf, 0, 0)) == 0)
   {
      LM_ERR("Insertion failed\n");
      return PARSER_ERROR;
   }

   if (insert_after_b2b(msg, to_insert, l) == -1)
   {
      LM_ERR("Failed to insert string. String to insert [%s] | error_code=%d\n",to_insert,PARSER_ERROR);
      return PARSER_ERROR;
   }

   return OK;
}

/******************************************************************************
 *        NAME: delete_b2b
 * DESCRIPTION: This function will delete the content of the sdp
 *
 *              msg :           sip message to manipulate.
 *              position :      initial position to begin deletion
 *              to_del :        number of characters to delete
 *
 *****************************************************************************/

int
delete_b2b(struct sip_msg *msg, char * position, int to_del)
{
   char * body;
   body = get_body(msg);
   struct lump* l;

   if ((l = del_lump(msg, position - msg->buf, to_del, 0)) == 0)
   {
      LM_ERR("Error, Deletion failed. error_code=%d\n",PARSER_ERROR);
      return PARSER_ERROR;
   }

   return OK;
}

/******************************************************************************
 *        NAME: replace_b2b
 * DESCRIPTION: This function will replace the sdp content.
 *		
 *		msg : 		sip message to manipulate.
 *		position : 	initial position to begin replacement
 *		to_del :	number of characters to replace
 *		to_insert :	string to insert
 *              
 *****************************************************************************/

int
replace_b2b(struct sip_msg *msg, char * position, int to_del, char * to_insert)
{
   char * body;
   body = get_body(msg);
   struct lump* l;

   if ((l = del_lump(msg, position - msg->buf, to_del, 0)) == 0)
   {
      LM_ERR("Error. del_lump failed. error_code=%d\n",PARSER_ERROR);
      return PARSER_ERROR;
   }

   if (insert_after_b2b(msg, to_insert, l) != OK)
   {
      LM_ERR("Failed to insert string. String to insert [%s]| error_code=%d\n",to_insert,PARSER_ERROR);
      return PARSER_ERROR;
   }

   return OK;
}

/******************************************************************************
 *        NAME: get_response_status
 * DESCRIPTION: This function parse a response to the command 'create'.
 *				Retrieves the call-id and status present in the response.
 *
 *		buffer : Message to parse and retrieve status.
 *		connection : Connection relative to this message response.
 *****************************************************************************/

int
get_response_status(char * buffer, conn * connection)
{
   int status = XCODER_CMD_ERROR;
   char response[64];
   bzero(response, 32);
   int i = 0;
   while (buffer[i] != '\0')
   {
      status = read_until_char(buffer, &i, '=', response); //Read word until character '='
      if (strcmp(response, "b2b_call_id") == 0)
      {
         i++; //Increment counter to advance character '='
         bzero(response, 64);
         read_until_end(buffer, &i, response); // Read until end of line
         connection->xcoder_id = atoi(response);
         if (connection->xcoder_id != connection->id)
         {
            LM_ERR("ERROR. Different connection call id and xcoder_call_id.b2bcallid=%d | conn_state=%d | call_id=%d | error_code=%d\n",
                  connection->id, connection->s, connection->xcoder_id, XCODER_CMD_ERROR);
            return XCODER_CMD_ERROR;
         }
         else
        	LM_NOTICE("Received response to create call: b2bcallid=%d | xcallid=%d",connection->id,connection->xcoder_id);
      }

      if (strcmp(response, "status") == 0)
      {
         i++; //Increment counter to advance character '='
         bzero(response, 64);
         read_until_end(buffer, &i, response); // Read until end of line
         LM_INFO("Status is : %s\n", response);

         if (strcmp(response, "OK") == 0)
         {
            status = OK;
         }
         else
         {
            LM_NOTICE("ERROR: Bad response from xcoder : %s\n", response);
            return XCODER_CMD_ERROR;
         }
      }


      bzero(response, 64); // Clean response
      move_to_end(buffer, &i); //Move to newline
   }

   return status;
}

/******************************************************************************
 *        NAME: get_ports_xcoder
 * DESCRIPTION: This function parse the response to the command 'get_ports' and retrieves the port send by xcoder.
 *
 * 		response : Message to parse to retrieve port number.
 * 		port :	Port number retrieved by this function.
 *****************************************************************************/

int
get_ports_xcoder(char * response, char * port)
{
   int status = XCODER_CMD_ERROR;
   int pos = 0;
   char att_temp[20];
   char value_xcoder[20];

   //Remove \r and \n from sip message

   char msg_copy[strlen(response)];
   int size=sizeof(msg_copy)/sizeof(char);
   if( size >= (strlen(response)) )
   {
	   snprintf(msg_copy,strlen(response)+1,response);
	   remove_newline_str(msg_copy);
   }
   else
   {
	   LM_WARN("Failed to allocate space for 'msg_copy' variable\n");
   }


   while (response[pos] != '\0')
   {
      if (response[pos] == '\n' && response[pos + 1] != '\0')
      {
         pos++;
         bzero(att_temp, 20);
         bzero(value_xcoder, 20);

         read_until_char(response, &pos, '=', att_temp);
         if (strcmp(att_temp, "adst_port") == 0)
         {
            pos++;
            get_word(response, &pos, port);
            if (port != NULL)
            {
               LM_INFO("Port retrieved is %s\n", port);
               return OK;
            }
            else
            {
               LM_ERR("ERROR. Error retrieving port. Response %s | error_code=%d\n",msg_copy,XCODER_CMD_ERROR);
               return XCODER_CMD_ERROR;
            }
         }
         else if (strcmp(att_temp, "status") == 0)
         {
            pos++;
            get_word(response, &pos, value_xcoder);
            if ((strcmp(value_xcoder, "OK") == 0))
            {
               LM_INFO("Message status is %s\n", value_xcoder);
            }
            else
            {
               LM_ERR("Error. Wrong status value : %s | Response %s | error_code=%d \n", value_xcoder, msg_copy, XCODER_CMD_ERROR);
               status = XCODER_CMD_ERROR;
            }
         }
         else if (strcmp(att_temp, "error_id") == 0)
         {
            pos++;
            get_word(response, &pos, value_xcoder);
            LM_ERR("ERROR. xcoder error_id : %s | Response %s\n", value_xcoder, msg_copy);

            if ((strcmp(value_xcoder, "3") == 0))
               return SERVER_UNAVAILABLE;
            else
               return XCODER_CMD_ERROR;
         }
      }
      pos++;
   }

   LM_ERR("Error : No port retrieved. Response %s | error_code=%d\n", msg_copy, status);
   return status;
}


/******************************************************************************
 *        NAME: read_from_xcoder
 * DESCRIPTION: This function reads a response from xcoder.
 *              It has a time out value (TIMEOUT), that is the maximum time that this function wait for a xcoder response.
 *
 *              to_read : Characters read will be stored in this variable
 *              socket : socket_list structure pointer that has the file descriptor.
 *              time_out : maximum time value to wait for a message in input buffer.
 *****************************************************************************/

int
read_from_xcoder(char * to_read, socket_list * socket,int timeout)
{
   LM_INFO("Starting to read on fd %d\n", socket->fd);

   int read_init_time = time(NULL);
   long time_elapsed = 0;

   int epfd = epoll_create(1); //This descriptor can be closed with close()
   if (epfd < 0)
   {
      LM_ERR("ERROR. Error epoll_create: errno [%d] | strerror [%s]\n", errno, strerror(errno));
      return SOCKET_ERROR;
   }
   static struct epoll_event ev;
   struct epoll_event events;

   ev.events = EPOLLIN;
   ev.data.fd = socket->fd;

   int res = epoll_ctl(epfd, EPOLL_CTL_ADD, socket->fd, &ev);
   if (res < 0)
   {
      LM_ERR("ERROR. Error epoll_ctl: errno [%d] | strerror [%s]\n", errno, strerror(errno));
      epoll_ctl(epfd, EPOLL_CTL_DEL, socket->fd, &ev);
      close(epfd);
      return SOCKET_ERROR;
   }

   LM_INFO("Set waiting socket %d\n", socket->fd);
   int number_tries = 0;
   while (1)
   {
      int n = epoll_wait(epfd, &events, MAX_SOCK_FD + 1, timeout);
      char message[XCODER_MAX_MSG_SIZE];
      char read_tmp[XCODER_MAX_MSG_SIZE];

      bzero(read_tmp, XCODER_MAX_MSG_SIZE);
      bzero(message, XCODER_MAX_MSG_SIZE);

      LM_INFO("Epoll unblocked\n");

      if (n < 0)
      {
         LM_NOTICE("No file descriptor ready to read. Epoll failed in fd %d | error_code=%d\n", socket->fd,XCODER_CMD_ERROR);
         epoll_ctl(epfd, EPOLL_CTL_DEL, socket->fd, &ev);
         close(epfd);
         return XCODER_CMD_ERROR;
      }
      else if (n == 0)
      {
         number_tries++;
         if (number_tries == 3)
         {
            time_elapsed = (time(NULL) - read_init_time);
            LM_INFO("Xcoder timeout in fd %d | time %ld | error_code=%d\n", socket->fd, time_elapsed,XCODER_TIMEOUT);
            epoll_ctl(epfd, EPOLL_CTL_DEL, socket->fd, &ev);
            close(epfd);
            return XCODER_TIMEOUT;
         }
         else
         {
            continue;
         }
      }
      else
      {
         int size = read(socket->fd, to_read, XCODER_MAX_MSG_SIZE);

         if (size < 0)
         {
            LM_ERR("No information readed. error_code=%d\n",XCODER_CMD_ERROR);
            epoll_ctl(epfd, EPOLL_CTL_DEL, socket->fd, &ev);
            close(epfd);
            return XCODER_CMD_ERROR;
         }
         else if (size > 0)
         {
            LM_INFO("Input found in fd %d!\n", socket->fd);

            sprintf(message, to_read); ///Temporarily assign to 'message'

            while (read(socket->fd, read_tmp, XCODER_MAX_MSG_SIZE) > 0)
            {
               LM_INFO("Message incomplete. Readed more in fd %d\n", socket->fd);
               sprintf(to_read, "%s%s", message, read_tmp);
               sprintf(to_read, message);
               bzero(read_tmp, XCODER_MAX_MSG_SIZE);
            }
         }
         else
         {
            LM_ERR("No input for fd %d | error_code=%d\n", socket->fd,XCODER_CMD_ERROR);
            epoll_ctl(epfd, EPOLL_CTL_DEL, socket->fd, &ev);
            close(epfd);
            return XCODER_CMD_ERROR;
         }
      }
      epoll_ctl(epfd, EPOLL_CTL_DEL, socket->fd, &ev);
      close(epfd);
      return OK;
   }

   epoll_ctl(epfd, EPOLL_CTL_DEL, socket->fd, &ev);
   close(epfd);
   return OK;
}

int
get_socket(void)
{
   int i, j = 0;
   lock_get(socket_lock);
   for (i = 0; i < MAX_SOCK_FD; i++)
   {
      j = (*socket_last_empty) + i;
      j = j % MAX_SOCK_FD;

      if (fd_socket_list[j].busy == 0)
      {
         LM_INFO("Found empty file descriptor. Pos %d | fd %d | busy %d\n", j, fd_socket_list[j].fd, fd_socket_list[j].busy);
         fd_socket_list[j].busy = 1;
         fd_socket_list[j].id = i;
         break;
      }
   }
   if (j < MAX_SOCK_FD)
   {
      *socket_last_empty = (j + 1) % MAX_SOCK_FD;

      lock_release(socket_lock);
      return j;
   }
   else
   {
      LM_ERR("ran %d positions without finding a free one. error_code=%d\n", i,GENERAL_ERROR);

      lock_release(socket_lock);
      return GENERAL_ERROR;
   }
   lock_release(socket_lock);
   return GENERAL_ERROR;
}

/******************************************************************************
 *        NAME: talk_to_xcoder
 * DESCRIPTION: Sends a message to xcoder and retreive the answer.
 *				For this, find an empty fd, and use it to talk with xcoder
 *
 *		to_send : Message to sent to xcoder
 *		received : Message received by xcoder.
 *		message_number : msg_count number. It is used to verify if the command to xcoder and the response from
 *						 xcoder have the same message_number.
 *****************************************************************************/

int
talk_to_xcoder(char * to_send,int message_number, char * received)
{
   int status = OK;
   socket_list * socket = NULL;

   int sock_index = get_socket();
   if (sock_index < 0)
   {
      LM_ERR("Error getting socket. error_code=%d\n",GENERAL_ERROR);
      return GENERAL_ERROR;
   }
   socket = &(fd_socket_list[sock_index]);

   int n = 0;
   int len = strlen(to_send);

   // Check if input buffer is empty.
   char buffer[XCODER_MAX_MSG_SIZE];
   bzero(buffer, XCODER_MAX_MSG_SIZE);

   LM_INFO("Checking if input buffer is emty. fd %d\n",socket->fd);
   while( (status=read_from_xcoder(buffer, socket,0)), status==OK )
   {
	   char read_copy[strlen(buffer)+1];
	   int size=sizeof(read_copy)/sizeof(char);
	   if( size >= (strlen(buffer)+1 ))
	   {
		   snprintf(read_copy,strlen(buffer)+1,buffer);
		   remove_newline_str(read_copy);
		   LM_ERR("ERROR. Buffer was not empty. Message read : %s. Proceeding to send message.\n",read_copy);
	   }
	   else
	   {
		   LM_ERR("ERROR. Buffer was not empty. Failed to allocate space for 'read_copy' variable\n");
	   }

   }

   lock_get(socket_lock);

   switch(status)
   {
   	  case SOCKET_ERROR : LM_ERR("Failed to check if buffer is empty, fd %d. error_code=%d. Trying to send message\n", socket->fd,status);
   	  	  	  	  	  	  break;
   	  case XCODER_CMD_ERROR : LM_INFO("Buffer is empty. fd %d. Proceeding to send message\n",socket->fd);
   	  	  	  	  	  	  	  break;
   	  case XCODER_TIMEOUT : LM_INFO("Buffer is empty, reached timeout value without reading, fd %d. Proceeding to send message\n",socket->fd);
   	  	  	  	  	  	  	  break;
   	  default : LM_INFO("Buffer is empty, fd %d. Proceeding to send message\n",socket->fd);
   	  	  	  	  break;
   }

   lock_release(socket_lock);

   char cmd_copy[strlen(to_send)+2];
   int size=sizeof(cmd_copy)/sizeof(char);
   if( size >= (strlen(to_send)+2) )
   {
	   snprintf(cmd_copy,strlen(to_send)+1,to_send);
	   remove_newline_str(cmd_copy);
	   LM_NOTICE("Command to xcoder. body [%s] | fd [%d]\n",cmd_copy,socket->fd);
   }
   else
   {
	   LM_WARN("Failed to allocate space for 'cmd_copy' variable\n");
   }

   n = write(socket->fd, to_send, len);

   if (n < len)
   {
      LM_ERR("ERROR. Wrote %d characters in fd %d. Value that should be writen %d | String to be writen %s | error_code=%d\n", n, socket->fd, len,cmd_copy,XCODER_CMD_ERROR);
      lock_get(socket_lock);
      socket->busy = 0;
      lock_release(socket_lock);
      return XCODER_CMD_ERROR;
   }

   LM_INFO("Characters to be writen %d. Characters wrote %d in fd %d\n", len, n, socket->fd);
   bzero(buffer, XCODER_MAX_MSG_SIZE);

   status = read_from_xcoder(buffer, socket,10000);
   if (status != OK)
   {
      LM_ERR("Error reading from xcoder. fd %d | Errcode %d\n",socket->fd,status);
      lock_get(socket_lock);

      switch(status)
      {
      	  case SOCKET_ERROR : LM_ERR("Setting to not busy fd %d. error_code=%d\n", socket->fd,status);
      	  	  	  	  	  	  break;
      	  case XCODER_CMD_ERROR : LM_ERR("ERROR. Error reading from xcoder. fd %d | error_code=%d\n.",socket->fd,status);
      	  	  	  	  	  	  	  break;
      	  case XCODER_TIMEOUT : LM_ERR("ERROR. Error reading from xcoder, timeout value reached. fd %d | error_code=%d\n",socket->fd,status);
      	  	  	  	  	  	  	  break;
      	  default : LM_ERR("ERROR. Error reading from xcoder. fd %d | error_code=%d\n.",socket->fd,status);
      	  	  	  	  break;
      }
      socket->busy = 0;

      lock_release(socket_lock);
      return status;
   }

   sprintf(received, buffer);

   char recv_copy[strlen(buffer)+2];
   int size_copy= sizeof(recv_copy)/sizeof(char);
   if( size_copy >= (strlen(buffer)+2) )
   {
	   snprintf(recv_copy,strlen(buffer)+1,buffer);
	   remove_newline_str(recv_copy);
   }
   else
   {
	   LM_WARN("Failed to allocate space for 'att_copy' variable\n");
   }


   // Check if msg_count received is the same msg_count in the message sent
   char value[128];
   bzero(value, 128);
   get_value_from_str("msg_count",received,value);
   int rcv_msg_count = atoi(value);
   LM_INFO("Checking msg_count. Send %d | Received %d\n",message_number,rcv_msg_count);

   if(rcv_msg_count!=message_number)
   {
	   LM_ERR("ERROR. msg_count sent does not match with msg_count received. Sent %d | Received %d | Message [%s].",message_number,rcv_msg_count,recv_copy);
	   return XCODER_CMD_ERROR;
   }

   LM_NOTICE("received from xcoder. body [%s] | fd [%d]\n", recv_copy,socket->fd);
   lock_get(socket_lock);

   LM_INFO("Setting to not busy fd %d\n", socket->fd);
   socket->busy = 0;

   lock_release(socket_lock);

   return status;
}

/******************************************************************************
 *        NAME: free_ports_client
 * DESCRIPTION: Sends a command to xcoder to free ports.
 *              Xcoder receives a client id and frees the allocated port for that client
 *
 *              cli : Client to free ports.
 *****************************************************************************/

int
free_ports_client(client * cli)
{
   int status = OK;
   char buffer_sent[XCODER_MAX_MSG_SIZE];
   char buffer_recv[XCODER_MAX_MSG_SIZE];
   bzero(buffer_sent, XCODER_MAX_MSG_SIZE);
   bzero(buffer_recv, XCODER_MAX_MSG_SIZE);

   int message_number = 0;
   get_and_increment(message_count, &message_number); // Store message count in message number and increment message_count, counts the number of communications with xcoder

   sprintf(buffer_sent, "xcoder/1.0\r\nmsg_type=command\r\nmsg_value=free_ports\r\nclient_id=%d\r\nmsg_count=%d\r\n<EOM>\r\n", cli->id,
         message_number); // Command to send to xcoder

   status = talk_to_xcoder(buffer_sent, message_number,buffer_recv);

   if (status != OK)
   {
      LM_NOTICE("Error interacting with xcoder. Failed to free ports\n");
      return status;
   }

   status = XCODER_CMD_ERROR;
   char response[64];
   bzero(response, 64);
   int i = 0;
   while (buffer_recv[i] != '\0')
   {
      status = read_until_char(buffer_recv, &i, '=', response); //Read word until character '='

      if (strcmp(response, "status") == 0)
      {
         i++; //Increment counter to advance character '='
         bzero(response, 64);
         read_until_end(buffer_recv, &i, response); // Read until end of line
         LM_INFO("Status is : %s\n", response);

         if (strcmp(response, "OK") == 0)
         {
            status = OK;
            break;
         }
         else
         {
            LM_ERR("ERROR: Bad response from xcoder : %s\n", response);
            return XCODER_CMD_ERROR;
         }
      }

      bzero(response, 64); // Clean response
      move_to_end(buffer_recv, &i); //Move to newline
   }
   return status;
}

/***************************************************************************************************************
 *        NAME: get_port_b2b
 * DESCRIPTION: This function sends a 'get_ports' command to xcoder and retrieves the value out of the response.
 *
 * 		cli : Client that will use the port requested to xcoder.
 * 		port : Will hold the port reserved by xcoder.
 *****************************************************************************************************************/

int
get_port_b2b(client * cli, char * port)
{
   char buffer_sent[XCODER_MAX_MSG_SIZE];
   bzero(buffer_sent, XCODER_MAX_MSG_SIZE);
   char buffer_recv[XCODER_MAX_MSG_SIZE];
   bzero(buffer_recv, XCODER_MAX_MSG_SIZE);

   int status = OK;
   int message_number = 0;
   get_and_increment(message_count, &message_number); // Store message count in message number and increment message_count, counts the number of communications with xcoder

   sprintf(buffer_sent,"xcoder/1.0\r\nmsg_type=command\r\nmsg_value=get_ports\r\nmsg_count=%d\r\nmedia_type=%c\r\nclient_id=%d\r\n<EOM>\r\n",
         message_number, cli->media_type[0], cli->id); // Command to send to xcoder

   status = talk_to_xcoder(buffer_sent, message_number,buffer_recv);

   if (status != OK)
   {
      LM_NOTICE("Error interacting with xcoder.\n");
      return status;
   }

   status = get_ports_xcoder(buffer_recv, port); // Parse the message received and retrieve the ports allocated by xcoder
   return status;
}

/****************************************************************************************
 *        NAME: get_conn_session
 * DESCRIPTION: This function searches for the next empty slot in the connections array.
 * 				Founded the position, it is stored in the variable received as an argument.
 *
 *		session : Variable that will hold the position found by get_conn_session function.
 ****************************************************************************************/

int
get_conn_session(int * session)
{
   int i, j = 0;
   LM_INFO("Get conn session index\n");

   for (i = 0; i < MAX_CONNECTIONS; i++)
   {
      j = (*conn_last_empty) + i;
      j = j % MAX_CONNECTIONS;
      lock_get(init_lock);

      if (connections[j].s == EMPTY || connections[j].s == TERMINATED)
      {
         int c = 0;
         for (c = 0; c < MAX_CLIENTS; c++) // Checks if all clients are empty
         {
            if (connections[j].clients[c].is_empty == 1)
            {
               LM_ERR("Error: Empty connection has non-empty clients. b2bcallid=%d | conn_state=%d | call_id=%s | client_id=%d | username=%s | tag=%s | src_ip %s | error_code=%d\n",
                     connections[j].id, connections[j].s, connections[j].call_id, connections[j].clients[c].id, connections[j].clients[c].user_name, connections[j].clients[c].tag, connections[j].clients[c].src_ip,SERVER_INTERNAL_ERROR);

               cancel_connection(&(connections[i]));
               clean_connection(&(connections[i]));
               return SERVER_INTERNAL_ERROR;
            }
         }
         LM_INFO("Found empty session on slot %d\n",j);
         connections[j].s = INITIALIZED;
         break;
      }
      lock_release(init_lock);
   }
   if (j < MAX_CONNECTIONS)
   {
      *conn_last_empty = (j + 1) % MAX_CONNECTIONS;
      *session = j;
      lock_release(init_lock);
   }
   else
   {
      LM_ERR("No empty connection found. error_code=%d\n",SERVER_UNAVAILABLE);
      return SERVER_UNAVAILABLE;
   }

   return OK;
}

/******************************************************************************
 *        NAME: readline_C
 * DESCRIPTION: This function will parse the attribute lines, line started by 'c='
 *              Parse and manipulate ip address present in this line.
 *
 *		msg : sip_msg structure representing the message received.
 *		sdp_origin : Original sdp body message received.
 *		i : current position in parsing of the sdp message.
 *		connection : Connection.
 *		cli : client that sent the message
 *****************************************************************************/

int
readline_C(struct sip_msg *msg, char * sdp, int * i, conn * connection, client * cli)
{
   LM_INFO("Processing connection information line, b2bcallid=%d | client_id=%d | username=%s | state=%d\n",connection->id,cli->id,cli->user_name,cli->s);
   int spaces = 0; // Hold the number os space (' ') characters encounter by parser
   char ip_type[4];
   bzero(ip_type, 4);
   int pos = *i;
   char * message = get_body(msg);

   while (sdp[pos] != '\0' && sdp[pos] != '\n')
   {
      switch (sdp[pos])
      {
         case ' ':
            spaces++;
            pos++;
            switch (spaces)
            {
               case 1:
                  get_word(sdp, &pos, ip_type);
                  break;
               case 2:
                  if ((strcmp(ip_type, "IP4") == 0) || (strcmp(ip_type, "IP6") == 0))
                  {
                     //////////////// substitute ip ////////
                     int position_to_read = pos;
                     char conn_ip[25];
                     bzero(conn_ip, 25);
                     get_word(sdp, &position_to_read, conn_ip); // Get connection IP
                     LM_INFO("Original conn ip : %s. b2bcallid=%d | client_id=%d | username=%s\n", conn_ip,connection->id,cli->id,cli->user_name);
                     sprintf(cli->conn_ip, conn_ip);

                     int len_to_end = count_length_to_end(sdp, pos);
                     if(replace_b2b(msg, &(message[pos]), len_to_end, media_relay)!=OK)
                     {
                    	 LM_ERR("ERROR. Failed to insert media relay ip in the sip message body. Media relay ip %s. b2bcallid=%d | conn_state=%d | call_id=%s | client_id=%d | client state=%d | src_ip=%s | username=%s | error_code=%d\n",
                    			 media_relay,connection->id,connection->s,connection->call_id,cli->id,cli->s,cli->src_ip,cli->user_name,PARSER_ERROR);
                    	 return PARSER_ERROR;
                     }
                     LM_INFO("Inserted media relay ip %s. b2bcallid=%d | client_id=%d | username=%s\n",media_relay,connection->id,cli->id,cli->user_name);
                     *i = pos;
                     move_to_end(sdp, i);
                     return OK;
                  }
                  else
                  {
                     LM_ERR("Wrong IP type : %s. Conn id %d | conn_state=%d | conn call_id=%s | client_id=%d | client state=%d | src_ip=%s | username=%s |error_code=%d\n",
                    		 ip_type,connection->id,connection->s,connection->call_id,cli->id,cli->s,cli->src_ip,cli->user_name,PARSER_ERROR);
                     return PARSER_ERROR;
                  }
                  break;
               default:
                  break;
            }
            break;

         default:
            pos++;
            break;
      }
   }
   if (sdp[pos] == '\n')
      pos++;
   *i = pos;

   LM_INFO("Successfully connection iformation line. b2bcallid=%d | conn_state=%d | call_id=%s | client_id=%d | client state=%d | src_ip=%s | username=%s\n",
      		   connection->id,connection->s,connection->call_id,cli->id,cli->s,cli->src_ip,cli->user_name);
   return OK;
}

/******************************************************************************
 *        NAME: readline_O
 * DESCRIPTION: This function will parse the attribute lines, line started by 'o='
 *              Parse and manipulate ip address present in this line.
 *
 *		msg : sip_msg structure representing the message received.
 *		sdp_origin : Original sdp body message received.
 *		i : current position in parsing of the sdp message.
 *		connection : array of connections.
 *		cli : client that sent the message
 *****************************************************************************/

int
readline_O(struct sip_msg *msg, char * sdp, int * i, conn * connection, client * cli)
{
   LM_INFO("Processing owner line, b2bcallid=%d | client_id=%d | username=%s | client state=%d\n",connection->id,cli->id,cli->user_name,cli->s);
   char ip_type[4];
   char original_dst_ip[25];
   bzero(ip_type, 4);
   bzero(original_dst_ip, 25);
   int pos = *i;
   int spaces = 0;

   char * message = get_body(msg);

   while (sdp[pos] != '\0' && sdp[pos] != '\n')
   {
      switch (sdp[pos])
      {
         case ' ':
            spaces++;
            pos++; //Count spaces
            switch (spaces)
            {
               case 4:
                  get_word(sdp, &pos, ip_type);
                  LM_INFO("ip_type : %s. b2bcallid=%d | client_id=%d | username=%s | client state=%d\n", ip_type,connection->id,cli->id,cli->user_name,cli->s);
                  break; //If spaces==4 then retrieve iptype

               case 5: //If spaces==5 then retrieve and subst ip
                  if ((strcmp(ip_type, "IP4") == 0) || (strcmp(ip_type, "IP6") == 0))
                  {
                     //////////////// substitute and retrieve ip ////////
                     int len_to_end = count_length_to_end(sdp, pos);
                     if(replace_b2b(msg, &(message[pos]), len_to_end, media_relay)!=OK)
                     {
                    	 LM_ERR("ERROR. Failed to insert media relay ip in the sip message body. Media relay ip %s. b2bcallid=%d | conn_state=%d | call_id=%s | client_id=%d | client state=%d | src_ip=%s | username=%s | error_code=%d\n",
                    			 media_relay,connection->id,connection->s,connection->call_id,cli->id,cli->s,cli->src_ip,cli->user_name,PARSER_ERROR);
                    	 return PARSER_ERROR;
                     }
                     *i = pos;

                     move_to_end(sdp, i);
                     return OK;
                  }
                  else
                  {
                     LM_ERR("Wrong IP type : %s. b2bcallid=%d | conn_state=%d | call_id=%s | client_id=%d | client state=%d | src_ip=%s | username=%s | error_code=%d\n",
                    		 ip_type,connection->id,connection->s,connection->call_id,cli->id,cli->s,cli->src_ip,cli->user_name,PARSER_ERROR);
                     return PARSER_ERROR;
                  }
                  break;
               default:
                  break;
            }
            break;

         default:
            pos++;
            break;
      }
   }

   if (sdp[pos] == '\n')
      pos++;
   *i = pos;

   LM_INFO("Successfully processed owner line. b2bcallid=%d | conn_state=%d | call_id=%s | client_id=%d | client state=%d | src_ip=%s | username=%s\n",
   		   connection->id,connection->s,connection->call_id,cli->id,cli->s,cli->src_ip,cli->user_name);
   return OK;
}

/******************************************************************************
 *        NAME: readline_M
 * DESCRIPTION: This function will parse the attribute lines, line started by 'm='
 *              Parse and manipulate port, and codec list.
 *
 *		msg : sip_msg structure representing the message received.
 *		sdp_origin : Original sdp body message received.
 *		i : current position in parsing of the sdp message.
 *		connection : array of connections.
 *		cli : client that sent the message.
 *****************************************************************************/

int
readline_M(struct sip_msg *msg, char * sdp, int * i, conn * connection, client * cli)
{
   LM_INFO("Processing media description line, b2bcallid=%d | client_id=%d | username=%s | client state=%d\n",connection->id,cli->id,cli->user_name,cli->s);
   char tmp_call_type[10];
   char tmp_src_payloads[50];
   char port[7];
   char original_port[7];
   bzero(tmp_call_type, 10); //Temporary, substitute with connection->clients[n].media_type
   bzero(original_port, 7);
   bzero(tmp_src_payloads, 50);
   bzero(port, 7);

   char * message = get_body(msg);
   int status = OK;
   int len_to_token = 0, spaces = 0, pos = *i;

   while (sdp[pos] != '\0' && sdp[pos] != '\n')
   {
      switch (sdp[pos])
      {
         case '=':
            pos++;
            get_word(sdp, &pos, tmp_call_type);
            LM_INFO("Call type is %s. b2bcallid=%d | client_id=%d | username=%s\n", tmp_call_type,connection->id,cli->id,cli->user_name);
            insert_call_type(connection, cli, tmp_call_type); //Insert call type into structure
            break;

         case ' ':
            spaces++;
            pos++;

            switch (spaces)
            {
               case 1:
               {
                  len_to_token = count_lenght_to_next_token(sdp, pos);

                  switch (cli->s)
                  //TODO: Por numa funcao??
                  {
                     case TO_HOLD:
                        ;
                     case OFF_HOLD:
                        ;
                     case INVITE_C:
                        if ((strcmp(tmp_call_type, "audio") == 0))
                        {
                           LM_INFO("Retrieving audio port for first client. b2bcallid=%d | client_id=%d | username=%s\n",connection->id,cli->id,cli->user_name);
                           status = get_port_b2b(cli, port);
                           if (status != OK)
                           {
                              LM_ERR("Error retrieving port. b2bcallid=%d | conn_state=%d | call_id=%s | client_id=%d | client state=%d | src_ip=%s | username=%s | error_code=%d\n",
                            		  connection->id,connection->s,connection->call_id,cli->id,cli->s,cli->src_ip,cli->user_name,status);
                              return status;
                           }
                           LM_INFO("Port number returned by xcoder is %s. b2bcallid=%d | client_id=%d | username=%s\n",port,connection->id,cli->id,cli->user_name);
                           replace_b2b(msg, &(message[pos]), len_to_token, port);
                           sprintf(cli->dst_audio, port);
                        }
                        else if ((strcmp(tmp_call_type, "video") == 0))
                        {
                           move_to_end(sdp, &pos);
                           if (sdp[pos] == '\n')
                              pos++;
                           *i = pos;
                           LM_INFO("Video not supported, bypassing all remainig sdp lines\n");
                           return VIDEO_UNSUPPORTED;
                           // Video not available
                        }
                        else if( (strcmp(tmp_call_type, "image") == 0) )
                        {
                 		   LM_ERR("Found fax attributes in the message. This feature is not yet supported, error_code=%d\n",NOT_ACCEPTABLE);
                 		   return NOT_ACCEPTABLE;

                        }
                        else
                           LM_ERR("Different media type. Call type %s | b2bcallid=%d | conn_state=%d | call_id=%s | client_id=%d | client state=%d | src_ip=%s | username=%s | error_code=%d\n",
                        		   tmp_call_type,connection->id,connection->s,connection->call_id,cli->id,cli->s,cli->src_ip,cli->user_name,status);
                        break;
                     case ON_HOLD:
                        ;
                     case PENDING_EARLY_MEDIA_C:
                        ;
                     case EARLY_MEDIA_C:
                        ;
                     case TWO_OK:
                        if ((strcmp(tmp_call_type, "audio") == 0))
                        {
                           LM_INFO("Retrieving audio port for second client. b2bcallid=%d | client_id=%d | username=%s\n",connection->id,cli->id,cli->user_name);
                           status = get_port_b2b(cli, port);
                           if (status != OK)
                           {
                               LM_ERR("Error retrieving port. b2bcallid=%d | conn_state=%d | call_id=%s | client_id=%d | client state=%d | src_ip=%s | username=%s | error_code=%d\n",
                            		   connection->id,connection->s,connection->call_id,cli->id,cli->s,cli->src_ip,cli->user_name,status);
                              return status;
                           }
                           LM_INFO("Port number returned by xcoder is %s. b2bcallid=%d | client_id=%d | username=%s\n",port,connection->id,cli->id,cli->user_name);
                           replace_b2b(msg, &(message[pos]), len_to_token, port);
                           sprintf(cli->dst_audio, port);
                        }
                        else if ((strcmp(tmp_call_type, "video") == 0))
                        {
                           move_to_end(sdp, &pos);
                           if (sdp[pos] == '\n')
                              pos++;
                           *i = pos;
                           return VIDEO_UNSUPPORTED;
                           // Video not available
                        }
                        else
                            LM_ERR("Different media type. Call type %s | b2bcallid=%d | conn_state=%d | call_id=%s | client_id=%d | client state=%d | src_ip=%s | username=%s | error_code=%d\n",
                         		   tmp_call_type,connection->id,connection->s,connection->call_id,cli->id,cli->s,cli->src_ip,cli->user_name,status);
                        break;

                     default:
                         LM_ERR("Error, Wrong connection state. b2bcallid=%d | conn_state=%d | call_id=%s | client_id=%d | client state=%d | src_ip=%s | username=%s | error_code=%d\n",
                        		 connection->id,connection->s,connection->call_id,cli->id,cli->s,cli->src_ip,cli->user_name,status);
                         break;
                  }
                  get_word(sdp, &pos, original_port);
                  sprintf(cli->original_port, original_port);
                  LM_INFO("Original port is %s. b2bcallid=%d | client_id=%d | username=%s\n", original_port,connection->id,cli->id,cli->user_name);
                  break;
               }
               case 3:
               {
                  int init_position = pos; //Position of the first payload
                  int len_to_end = count_length_to_end(sdp, pos);
                  read_until_end(sdp, &pos, tmp_src_payloads);

                  switch (cli->s)
                  {
                     case TO_HOLD:
                        ;
                     case OFF_HOLD:
                        ;
                     case INVITE_C:
                        LM_INFO("Inserting payloads %s. b2bcallid=%d | client_id=%d | username=%s", tmp_src_payloads,connection->id,cli->id,cli->user_name);
                        insert_payloads(connection, cli, tmp_src_payloads);

                        char payload_lst[128];
                        bzero(payload_lst, 128);
                        get_all_suported_payloads(payload_lst);
                        replace_b2b(msg, &(message[init_position]), len_to_end, payload_lst);
                        break;

                     case ON_HOLD:
                        ;
                     case PENDING_EARLY_MEDIA_C:
                        ;
                     case EARLY_MEDIA_C:
                        ;
                     case TWO_OK:
                        LM_INFO("Inserting payloads %s. b2bcallid=%d | client_id=%d | username=%s\n", tmp_src_payloads,connection->id,cli->id,cli->user_name);
                        insert_payloads(connection, cli, tmp_src_payloads);

                        client * cli_dst = NULL;
                        get_client(connection, cli, &cli_dst); //Get destination client
                        if (cli_dst == NULL)
                        {
                           LM_ERR("ERROR, error getting destination client (NULL CLIENT). b2bcallid=%d | conn_state=%d | call_id=%s | client_id=%d | client state=%d | src_ip=%s | username=%s | error_code=%d\n",
                         		connection->id,connection->s,connection->call_id,cli->id,cli->s,cli->src_ip,cli->user_name,PARSER_ERROR);
                           return PARSER_ERROR;
                        }

                        replace_b2b(msg, &(message[init_position]), len_to_end, cli_dst->payload_str);
                        break;

                     default:
                        LM_ERR("No valid connection state.b2bcallid=%d | conn_state=%d | call_id=%s | client_id=%d | client state=%d | src_ip=%s | username=%s | error_code=%d\n",
                         		connection->id,connection->s,connection->call_id,cli->id,cli->s,cli->src_ip,cli->user_name,PARSER_ERROR);
                        return PARSER_ERROR;
                        break;
                  }

                  move_to_end(sdp, &pos);
                  *i = pos;
                  LM_INFO("Successfully processed media description line. b2bcallid=%d | conn_state=%d | call_id=%s | client_id=%d | client state=%d | src_ip=%s | username=%s\n",
                		  connection->id,connection->s,connection->call_id,cli->id,cli->s,cli->src_ip,cli->user_name);
                  return OK;
                  break;
               }
               default:
                  break;

            }
            break;
         default:
            pos++;
            break;
      }
   }
   if (sdp[pos] == '\n')
      pos++;
   *i = pos;

   LM_INFO("Successfully processed media description line. b2bcallid=%d | conn_state=%d | call_id=%s | client_id=%d | client state=%d | src_ip=%s | username=%s\n",
	connection->id,connection->s,connection->call_id,cli->id,cli->s,cli->src_ip,cli->user_name);

   return OK;
}

/******************************************************************************
 *        NAME: readline_A
 * DESCRIPTION: This function will parse the attribute lines, line started by 'a='
 *		Parse the codecs and attributes that later will be matched with the supported codecs of media relay.
 *
 *		msg : sip_msg structure representing the message received.
 *		sdp_origin : Original sdp body message received.
 *		i : current position in parsing of the sdp message.
 *		connection : array of connections.
 *		cli : client that sent the message.
 *****************************************************************************/

int
readline_A(struct sip_msg *msg, char * sdp, int * i, conn * connection, client * cli)
{
   LM_INFO("Starting to read attribute line. b2bcallid=%d | client_id=%d | username=%s | client state=%d\n",connection->id,cli->id,cli->user_name,cli->s);
   int begin = *i;
   int pos = *i;
   int status = OK;
   char type_temp[32];
   char payload_tmp[32];
   char codec[64];
   char * message = get_body(msg);

   while (sdp[pos] != '\0' && sdp[pos] != '\n')
   {
      if (sdp[pos] == '=')
      {
         pos++;
         bzero(type_temp, 32);
         bzero(payload_tmp, 32);
         bzero(codec, 64);

         if (read_until_char(sdp, &pos, ':', type_temp) != OK) //Read type of attribute, verify if is recognizable pattern
         {
            int len_to_end_tmp = count_length_to_end(sdp, begin);
            LM_INFO("Bypassing line : %.*s\n", len_to_end_tmp, &(sdp[begin]));
            LM_INFO("Attribute line different from pattern : a=[a-zA-Z0-9]+:.+\n");
            move_to_end(sdp, &begin);
            pos = begin;
            break;
         }

         pos++; //Advance the position ':'
         if (get_word(sdp, &pos, payload_tmp) != OK) //Read attribute value (payload)
         {
            LM_ERR("ERRO: Error while parsing attribute line. b2bcallid=%d | conn_state=%d | call_id=%s | client_id=%d | client state=%d | src_ip=%s | username=%s | error_code=%d\n",
            		connection->id,connection->s,connection->call_id,cli->id,cli->s,cli->src_ip,cli->user_name,PARSER_ERROR);
            return PARSER_ERROR;
         }

         if (strcmp(type_temp, "rtpmap") == 0)
         {
            pos++; //Advance space that exists after payload in attribute rtpmap line
            status = read_until_char(sdp, &pos, '/', codec);
            if (status != OK || (strlen(codec) < 1))
            {
               LM_ERR("ERROR. Error parsing codec for payload %s | b2bcallid=%d | conn_state=%d | call_id=%s | client_id=%d | client state=%d | src_ip=%s | username=%s | error_code=%d\n",
            		   payload_tmp,connection->id,connection->s,connection->call_id,cli->id,cli->s,cli->src_ip,cli->user_name,PARSER_ERROR);
               return PARSER_ERROR;
            }
            LM_INFO("Retrieved codec info. b2bcallid=%d | client_id=%d | codec=%s | payload=%s.\n",connection->id,cli->id,codec,payload_tmp);
         }

         if (strcmp(type_temp, "rtpmap") == 0 || strcmp(type_temp, "fmtp") == 0)
         {
            int len_to_end = count_length_to_end(sdp, begin); //Calculate number of characters until end of line(\r or \n or \0)
            if (delete_b2b(msg, &(message[begin]), (len_to_end + 2)) != OK) //Delete characters until end of line (+2 means \r\n, the line termination)
            {
               LM_ERR("ERRO: Error while deleting in attribute line.b2bcallid=%d | conn_state=%d | call_id=%s | client_id=%d | client state=%d | src_ip=%s | username=%s | error_code=%d\n",
            		connection->id,connection->s,connection->call_id,cli->id,cli->s,cli->src_ip,cli->user_name,PARSER_ERROR);
               return PARSER_ERROR;
            }
            if (put_attribute(sdp, &begin, type_temp, payload_tmp, codec, cli->payloads) != OK) //Insert the attribute in the client structure
            {
               LM_ERR("ERROR: Error putting attribute.Type_temp %s | payload_tmp %s | b2bcallid=%d | conn_state=%d | call_id=%s | client_id=%d | client state=%d | src_ip=%s | username=%s | error_code=%d\n",
            		   type_temp, payload_tmp,connection->id,connection->s,connection->call_id,cli->id,cli->s,cli->src_ip,cli->user_name,PARSER_ERROR);
               return PARSER_ERROR;
            }
         }
         else if (strcmp(type_temp, "alt") == 0)
         {
            int len_to_end = count_length_to_end(sdp, begin); //Calculate number of characters until end of line(\r or \n or \0)
            if (delete_b2b(msg, &(message[begin]), (len_to_end + 2)) != OK) //Delete characters until end of line (+2 means \r\n, the line termination)
            {
               LM_ERR("ERRO: Error while deleting in attribute line. Type %s | length %d | b2bcallid=%d | conn_state=%d | call_id=%s | client_id=%d | client state=%d | src_ip=%s | username=%s | error_code=%d\n",
            		   type_temp,len_to_end,connection->id,connection->s,connection->call_id,cli->id,cli->s,cli->src_ip,cli->user_name,PARSER_ERROR);
               return PARSER_ERROR;
            }

         }
         else if (strcmp(type_temp, "ptime") == 0)
         {
            int len_to_end = count_length_to_end(sdp, begin); //Calculate number of characters until end of line(\r or \n or \0)
            if (delete_b2b(msg, &(message[begin]), (len_to_end + 2)) != OK) //Delete characters until end of line (+2 means \r\n, the line termination)
            {
               LM_ERR("ERRO: Error while deleting in attribute line. Type %s | length %d | b2bcallid=%d | conn_state=%d | call_id=%s | client_id=%d | client state=%d | src_ip=%s | username=%s | error_code=%d\n",
                     type_temp,len_to_end,connection->id,connection->s,connection->call_id,cli->id,cli->s,cli->src_ip,cli->user_name,PARSER_ERROR);
               return PARSER_ERROR;
            }

         }
         move_to_end(sdp, &begin);
         pos = begin;
         break;
      }
      pos++;
   }
   if (sdp[pos] == '\n')
      pos++;

   *i = pos;
   return 1;
}

/******************************************************************************
 *        NAME: readLine
 * DESCRIPTION: This function process a single sdp content line.
 *		Identify the type, and invoke the appropriate parse function.
 *
 *		msg : sip_msg structure representing the message received.
 *		sdp_origin : Original sdp body message received.
 *		i : current position in parsing of the sdp message.
 *		connection : array of connections.
 *		cli : client that sent the message.
 *
 *****************************************************************************/

int
readLine(struct sip_msg *msg, char * sdp, int * i, conn * connection, client * cli)
{
   LM_INFO("Read sdp line\n");
   int pos = *i;
   switch (sdp[pos])
   {
      case 'o':
         return (readline_O(msg, sdp, i, connection, cli));
         break;
      case 'c':
         if (cli->s == TO_HOLD || cli->s == ON_HOLD) //IF it is a client to put on hold or if already on hold, conn ip must stay 0.0.0.0
         {
            break;
         }
         return (readline_C(msg, sdp, i, connection, cli));
         break;
      case 'm':
         return (readline_M(msg, sdp, i, connection, cli));
         break;
      case 'a':
         return (readline_A(msg, sdp, i, connection, cli));
         break;
      default:
         break;
   }

   while (sdp[pos] != '\n' && sdp[pos] != '\0')
   {
      pos++;
   }

   if (sdp[pos] == '\n')
      pos++;

   *i = pos;
   return OK;
}

/******************************************************************************
 *        NAME: parse_sdp_b2b
 * DESCRIPTION: This function will parse the sdp content of a sip message,
 *		and define the modifications to make in the message.
 *
 *		msg : sip_msg structure representing the message received.
 *		sdp_origin : Original sdp body message received.
 *		connection : array of connections.
 *		cli : client that sent the message.
 *****************************************************************************/

int
parse_sdp_b2b(struct sip_msg *msg, char ** sdp_origin, conn * connection, client * cli)
{

   char * sdp = *sdp_origin;
   int i = 0;
   int j = 0;
   int begin_line = 0; //Holds the initial position in each line. In case is necessary to remove it.
   int status = OK;
   char * message = get_body(msg); //msg->buf;
   int body_lenght = msg->len - (get_body(msg) - msg->buf);

   /////////////////// Read line by line //////////////////////////////

   if (connection->s != PENDING && connection->s != TWO_OK)
   {
      LM_INFO("Cleaning client structure. b2bcallid=%d | client_id=%d | client_state=%d | username=%s\n",connection->id,cli->id,cli->s,cli->user_name);
      int l = 0;

      bzero(cli->original_port, 7);
      bzero(cli->media_type, 10);
      bzero(cli->dst_audio, 7);
      bzero(cli->dst_video, 7);
      bzero(cli->payload_str, 50);
      for (l = 0; l < MAX_PAYLOADS; l++)
      {
         bzero(cli->payloads[l].payload, 32);
         bzero(cli->payloads[l].codec, 64);
         bzero(cli->payloads[l].attr_rtpmap_line, 100);
         bzero(cli->payloads[l].attr_fmtp_line, 100);
         cli->payloads[l].is_empty = 0;
      }
   }

   while (sdp[i] != '\0')
   {
      begin_line = i;
      status = readLine(msg, *sdp_origin, &i, connection, cli);
      if (i >= body_lenght)
         break; //If we reach the end of message, break cycle.

      if (status != OK)
      {
         if (status == VIDEO_UNSUPPORTED)
         {
            ///////////// Deleting m=video line and remaining sdp content
            LM_INFO("Video is not supported. Deleting media video line from sdp content. b2bcallid=%d | client_id=%d | username=%s\n"
            		,connection->id,cli->id,cli->user_name);

            int to_del = msg->len - (&(message[begin_line]) - msg->buf);
            LM_INFO("Characters to delete in original message %d\n", to_del);

            if (delete_b2b(msg, &(message[begin_line]), to_del) != OK)
            {
               //Delete characters until end of line (+2 means \r\n, the line termination)
               LM_ERR("ERROR: Error while deleting media video line. b2bcallid=%d | conn_state=%d | call_id=%s | client_id=%d | client state=%d | src_ip=%s | username=%s | error_code=%d\n",
            		   connection->id,connection->s,connection->call_id,cli->id,cli->s,cli->src_ip,cli->user_name,PARSER_ERROR);
               return PARSER_ERROR;
            }

            status = OK;
         }
         else
         {
            LM_ERR("ERROR: Error in parsing. b2bcallid=%d | conn_state=%d | call_id=%s | client_id=%d | client state=%d | src_ip=%s | username=%s | error_code=%d\n",
            		connection->id,connection->s,connection->call_id,cli->id,cli->s,cli->src_ip,cli->user_name,status);
            return status;
         }
      }
   }

   //////////////////// Match payloads ////////////////////////
   char chosen_payload[32];
   bzero(chosen_payload, 32);

   status = match_payload(connection, cli, chosen_payload);
   if (status != OK)
   {
      LM_ERR("ERROR: while matching payload. b2bcallid=%d | conn_state=%d | call_id=%s | client_id=%d | client state=%d | src_ip=%s | username=%s | error_code=%d\n",
    		  connection->id,connection->s,connection->call_id,cli->id,cli->s,cli->src_ip,cli->user_name,status);
      return status;
   }

   ////////////////// Manipulate codecs/payloads according to connection state ///////////////////////
   switch (cli->s)
   {
      case TO_HOLD:
         ;
         /* no break */
      case OFF_HOLD:
         ;
         /* no break */
      case INVITE_C:
      {
         char dst_att[XCODER_MAX_MSG_SIZE];
         bzero(dst_att, XCODER_MAX_MSG_SIZE);
         get_all_supported_att(dst_att);
         insert_b2b(msg, &(msg->buf[msg->len]), dst_att); //Insert codecs/payloads suported by the media relay in the sdp content
         break;
      }
      case ON_HOLD:
         ;
         /* no break */
      case PENDING_EARLY_MEDIA_C:
         ;
         /* no break */
      case EARLY_MEDIA_C:
         ;
         /* no break */
      case TWO_OK:
      { // Insert chosen payload
         if (cli->dst_ip == '\0' || cli->dst_ip == NULL)
         {
            LM_ERR("ERROR: Empty destination ip. b2bcallid=%d | conn_state=%d | call_id=%s | client_id=%d | client state=%d | src_ip=%s | username=%s | error_code=%d\n",
            		connection->id,connection->s,connection->call_id,cli->id,cli->s,cli->src_ip,cli->user_name,PARSER_ERROR);
            return PARSER_ERROR;
         }

         client * cli_dst = NULL;
         get_client(connection, cli, &cli_dst); // Get destination client to insert the chosen payload
         if (cli_dst == NULL)
         {
            LM_ERR("ERROR: No destination client encountered. b2bcallid=%d | conn_state=%d | call_id=%s | client_id=%d | client state=%d | src_ip=%s | username=%s | error_code=%d\n",
            		connection->id,connection->s,connection->call_id,cli->id,cli->s,cli->src_ip,cli->user_name,PARSER_ERROR);
            return PARSER_ERROR;
         }

         for (j = 0; j < MAX_PAYLOADS; j++) // Inserting chosen payload in the sdp content
         {
            if (cli_dst->payloads[j].is_empty == 1)
            {
               insert_b2b(msg, &(msg->buf[msg->len]), cli_dst->payloads[j].attr_rtpmap_line);
               insert_b2b(msg, &(msg->buf[msg->len]), cli_dst->payloads[j].attr_fmtp_line);
            }
         }
         break;
      }

      default:
         LM_ERR("ERROR: Wrong connection state. b2bcallid=%d | conn_state=%d | call_id=%s | client_id=%d | client state=%d | src_ip=%s | username=%s | error_code=%d\n",
        		 connection->id,connection->s,connection->call_id,cli->id,cli->s,cli->src_ip,cli->user_name,PARSER_ERROR);
         return PARSER_ERROR;
         break;
   }
   LM_INFO("Successfully parsed sdp message. b2bcallid=%d | conn_state=%d | call_id=%s | client_id=%d | client state=%d | src_ip=%s | username=%s\n"
		   ,connection->id,connection->s,connection->call_id,cli->id,cli->s,cli->src_ip,cli->user_name);
   //Successfully parsed client

   return status;
}

/******************************************************************************
 *        NAME: init_payloads
 * DESCRIPTION: Initializes payload_struct structure.
 *
 * 		payloads : payload_struct to be initialized.
 *****************************************************************************/

int
init_payloads(payload_struct * payloads)
{
   int i = 0;
   if (payloads == NULL)
   {
      LM_ERR("ERROR: Null payload_struct received. error_code=%d\n",INIT_ERROR);
      return INIT_ERROR;
   }
   for (i = 0; i < MAX_PAYLOADS; i++)
   {
      bzero(payloads->payload, 32);
      bzero(payloads->codec, 64);
      bzero(payloads->attr_rtpmap_line, 100);
      bzero(payloads->attr_fmtp_line, 100);
      payloads[i].is_empty = 0;
   }
   if (i != MAX_PAYLOADS)
   {
      LM_ERR("ERROR: error initializing payload structure. error_code=%d\n",INIT_ERROR);
      return INIT_ERROR;
   }

   return OK;
}

/******************************************************************************
 *        NAME: init_structs
 * DESCRIPTION: This functions initializes the structures.
 *****************************************************************************/

int
init_structs(void)
{
   int i = 0;
   int status = OK;
   for (i = 0; i < MAX_CONNECTIONS; i++)
   {
      connections[i].s = EMPTY;
      connections[i].id = -1;
      connections[i].xcoder_id = -1;
      bzero(connections[i].call_id, 128);
      bzero(connections[i].b2b_client_callID, 128);
      bzero(connections[i].b2b_client_serverID, 128);
      bzero(connections[i].b2b_key, 128);
      bzero(connections[i].cseq, 128);

      int k = 0;
      for (k = 0; k < MAX_CLIENTS; k++)
      {
         connections[i].clients[k].id = -1;
         bzero(connections[i].clients[k].src_ip, 25);
         bzero(connections[i].clients[k].dst_ip, 25);
         bzero(connections[i].clients[k].user_name, 128);
         bzero(connections[i].clients[k].user_agent, 128);
         bzero(connections[i].clients[k].conn_ip, 25);
         bzero(connections[i].clients[k].tag, 128);
         bzero(connections[i].clients[k].b2b_tag, 128);
         bzero(connections[i].clients[k].b2b_tag, 128);
         bzero(connections[i].clients[k].original_port, 7);
         bzero(connections[i].clients[k].media_type, 10);
         bzero(connections[i].clients[k].dst_audio, 7);
         bzero(connections[i].clients[k].dst_video, 7);
         bzero(connections[i].clients[k].payload_str, 50);
         status = init_payloads(connections[i].clients[k].payloads);
         if (status != OK)
         {
            LM_ERR("ERROR. Error initializing client payload structure. error_code=%d\n",INIT_ERROR);
            return INIT_ERROR;
         }
         connections[i].clients[k].s = EMPTY;
         connections[i].clients[k].is_empty = 0;
      }
   }

   if (i != MAX_CONNECTIONS || status != OK)
   {
      LM_ERR("ERROR: error initializing structures. error_code=%d\n",INIT_ERROR);
      return INIT_ERROR;
   }

   for(i = 0; i < MAX_PAYLOADS; i++)
   {
      codecs[i].payload = -1;
      codecs[i].frequency = -1;
      codecs[i].channels = -1;
      bzero(codecs[i].name, 256);
      bzero(codecs[i].sdpname, 256);
      bzero(codecs[i].fmtp, 256);
      bzero(codecs[i].attr_rtpmap_line, 256);
      bzero(codecs[i].attr_fmtp_line, 256);
      codecs[i].is_empty = 0;
   }

   return OK;
}

int
free_xcoder_resources(char * callID)
{
	   int i = 0;
	   for (i = 0; i < MAX_CONNECTIONS; i++)
	   {
	      if (connections[i].s != TERMINATED && connections[i].s != EMPTY
	            && (strcmp(connections[i].call_id, callID) == 0 || strcmp(connections[i].b2b_client_callID, callID) == 0))
	      {
	         LM_INFO("Freeing resources. b2bcallid=%d | conn_state=%d | call_id=%s\n", connections[i].id, connections[i].s,callID);

	         if (connections[i].s == INITIALIZED || connections[i].s == PENDING)
	         {
	            free_ports_client(&(connections[i].clients[0]));
	            clean_connection(&(connections[i]));
	         }
	         else
	         {
	            LM_ERR("Wrong connection state. b2bcallid=%d |conn_state=%d | call_id=%s\n", connections[i].id, connections[i].s,callID);
	         }
	         break;
	      }
	   }
	   return OK;
}

/******************************************************************************
 *        NAME: add_b2b_callID
 * DESCRIPTION: This function is invoked in b2b_logic module. In this module are generated
 *              new call-id and tags for the clients, and this functions adds them in the structures.
 *		Receives two ids ( orig_callID, original_tag) to identify  the connection and client.
 * 		Receives also the generated ids to be added in the structures.
 *
 *		b2b_callID -> 		call-id generated by the b2b_logic module.
 * 		b2b_server_callID ->	Id generated by b2b_logic. B2B module use this id to identify callee tag.
 *		b2b_key ->		Id generated by b2b_logic. This id represent the dialog, later used to cancel the dialog.
 *		b2b_generated_tag -> 	Tag generated by b2b_logic module that identifies caller.
 *****************************************************************************/

int
add_b2b_callID(char * orig_callID, char * b2b_callID, char * b2b_server_callID, char * b2b_key, char * original_tag,
      char * b2b_generated_tag)
{
   LM_NOTICE("Adding b2b call id. Original call ID=%s | b2b gen_call id=%s | b2b server id=%s | b2b_key=%s | original_tag=%s | b2b_generated_tag=%s\n",
         orig_callID, b2b_callID, b2b_server_callID, b2b_key, original_tag, b2b_generated_tag);
   int i = 0;
   int count = 0; //Count the number of matches
   for (i = 0; i < MAX_CONNECTIONS; i++)
   {
      if ((connections[i].s != TERMINATED || connections[i].s != EMPTY) && (strcmp(connections[i].call_id, orig_callID) == 0))
      {
         if (count == 0)
         {
            LM_INFO("Found connection. b2bcallid=%d | call_id=%s\n", connections[i].id,connections[i].call_id);

            bzero(connections[i].b2b_client_callID, 128);
            bzero(connections[i].b2b_client_serverID, 128);
            bzero(connections[i].b2b_key, 128);

            memcpy(&(connections[i].b2b_client_callID), b2b_callID, strlen(b2b_callID));
            memcpy(&(connections[i].b2b_client_serverID), b2b_server_callID, strlen(b2b_server_callID));
            memcpy(&(connections[i].b2b_key), b2b_key, strlen(b2b_key));

            /*sprintf(connections[i].b2b_client_callID,b2b_callID);
             sprintf(connections[i].b2b_client_serverID,b2b_server_callID);
             sprintf(connections[i].b2b_key,b2b_key);*/
         }
         else
         {
            LM_ERR("ERROR: Found more than one connection with the same call_iD. b2bcallid=%d | conn_state=%d | call_id=%s | error_code=%d\n",connections[i].id,connections[i].s,connections[i].call_id,DUPLICATE_CLIENT);
            return DUPLICATE_CLIENT;
         }
         count++;
         int l = 0;
         for (l = 0; l < MAX_CLIENTS; l++)
         {
            if ((strcmp(connections[i].clients[l].tag, original_tag) == 0))
            {
               LM_NOTICE("Found client. client_id=%d\n",connections[i].clients[l].id);
               sprintf(connections[i].clients[l].b2b_tag, b2b_generated_tag);
            }
         }
      }
   }
   if (count == 0)
   {
      LM_ERR("ERROR: No connection found. call_id=%s | b2b_call_id=%s | error_code=%d\n",orig_callID,b2b_callID,INIT_ERROR);
      return INIT_ERROR;
   }

   return OK;
}

/*************************************************************************************************************
 *        NAME: parse_codecs
 * DESCRIPTION: This function receives a message containing all codecs supported by the media relay.
 * 				Parses the message, retrieves the codecs names, payloads and attributes.
 * 				Insert the newly retrieved codecs in the codecs structure.
 *
 *		message - The message to parse that contains all all codecs supported by the media relay.
 *************************************************************************************************************/

int
parse_codecs(char * message)
{
   char param_names[6][32] = { "status", "codec_name", "codec_sdpname", "codec_payload", "codec_frequency", "codec_fmtp" };
   int array_size = sizeof(param_names) / (sizeof(char) * 32);

   char param_name[32];
   char value[128];
   int i = 0;
   int j = 0;
   int status = PARSER_ERROR;
   int codec_index = -1; // Codec index in g_codecs array

   ////////// Parse message //////////////////
   while (message[i] != '\0')
   {
      bzero(param_name, 32);
      bzero(value, 128);
      read_until_char(message, &i, '=', param_name); // Read until '=' to retrieve parameter name
      i++; // Advance one position. Advance '='
      read_until_end(message, &i, value); // Read until end of line to retrieve parameter value

      for (j = 0; j < array_size; j++)
      {
         if (strcmp(param_names[j], param_name) == 0) // Compare parameter name with param_names array
         {
            LM_INFO("Index %d. Param : %s | Value : %s", j, param_name, value);
            switch (j)
            {
               case 0: // First parameter is status
                  if (strcmp(value, "OK") != 0)
                  {
                     LM_ERR("ERROR. Wrong status value. Message : %s | error_code=%d\n",message,PARSER_ERROR);
                     return PARSER_ERROR;
                  }
                  else
                     status = OK;

                  LM_INFO("Status = %s\n", value);
                  break;
               case 1: // Second parameter is codec name
                  codec_index++;
                  LM_INFO("codec_name = %s. Index %d\n", value, codec_index);
                  sprintf(codecs[codec_index].name, "%s", value);
                  codecs[codec_index].is_empty = 1;
                  break;
               case 2: // Third parameter is codec sdp name
                  LM_INFO("codec_sdpname = %s\n", value);
                  sprintf(codecs[codec_index].sdpname, "%s", value);
                  break;
               case 3: //Fourth parameter is payload
                  LM_INFO("codec_payload = %s\n", value);
                  codecs[codec_index].payload = atoi(value);
                  break;
               case 4: //Fifth parameter is codec frequency
                  LM_INFO("codec_frequency = %s\n", value);
                  codecs[codec_index].frequency = atoi(value);
                  break;
               case 5: //Sixth parameter is codec fmtp
                  LM_INFO("codec_fmtp = %s\n", value);
                  sprintf(codecs[codec_index].fmtp, "%s", value);
                  break;
               default:
                  LM_ERR("ERROR. Parameter '%s' not recognizable\n", param_name);
                  break;
            }

         }
      }
      LM_INFO("Param : %s | Value : %s\n", param_name, value);

      move_to_end(message, &i); // Move to end of line
   }
   int k = 0;
   for (k = 0; k < MAX_PAYLOADS; k++)
   {
      if (codecs[k].is_empty == 1)
      {
         bzero(codecs[k].attr_rtpmap_line, 100);
         bzero(codecs[k].attr_fmtp_line, 100);

         if (codecs[k].frequency != -1)
         {
            sprintf(codecs[k].attr_rtpmap_line, "a=rtpmap:%d %s/%d\r\n", codecs[k].payload, codecs[k].sdpname, codecs[k].frequency);
         }

         if ((strlen(codecs[k].fmtp)) > 0)
         {
            sprintf(codecs[k].attr_fmtp_line, "a=fmtp:%d %s\r\n", codecs[k].payload, codecs[k].fmtp);
         }
      }
   }

   return status;
}

/******************************************************************************
 *        NAME: send_remove_to_xcoder
 * DESCRIPTION: This function communicates with xcoder and sends a 'remove' command
 *				to end the current call.
 *              This functions receives as an argument the connection to terminate.
 *****************************************************************************/

int
send_remove_to_xcoder(conn * connection)
{
   int status = OK;
   char buffer_send[XCODER_MAX_MSG_SIZE];
   bzero(buffer_send, XCODER_MAX_MSG_SIZE);
   char buffer_recv[XCODER_MAX_MSG_SIZE];
   bzero(buffer_recv, XCODER_MAX_MSG_SIZE);
   int message_number = 0;
   get_and_increment(message_count, &message_number);

   sprintf(buffer_send, "xcoder/1.0\r\nmsg_type=command\r\nmsg_value=remove\r\nmsg_count=%d\r\nb2b_call_id=%d\r\n<EOM>\r\n", message_number,
         connection->id); // Command to send to xcoder

   status = talk_to_xcoder(buffer_send,message_number,buffer_recv);

   if (status != OK)
   {
      LM_ERR("Error interacting with xcoder. b2bcallid=%d | error_code=%d\n",connection->id,status);
      return status;
   }

   return OK;
}

/******************************************************************************
 *        NAME: send_reply_b2b
 * DESCRIPTION: This function will send a reply inside a b2b dialog
 *       call_id : Dialog call_id
 *       cli_fromtag : Original tag of the client that send the request
 *       cli_dst_b2b_tag : Tag generated by b2b of the destination client
 *       b2b_server_key : B2b server key
 *       et : entity type (B2B_SERVER=0, B2B_CLIENT, B2B_NONE)
 *       method : Method that originates the reply
 *       code : Error code
 *       text : Reason (description) of the error
 *
 *****************************************************************************/

int send_reply_b2b(char * call_id, char * cli_fromtag, char * cli_dst_b2b_tag, char * b2b_server_key, enum b2b_entity_type et, int method, int code, char * text)
{
   if(call_id==NULL || cli_fromtag==NULL || cli_dst_b2b_tag==NULL || b2b_server_key==NULL || text==NULL)
   {
      LM_NOTICE("ERROR. Critical parameters in send_reply_b2b function are empty.\n");
   }

   LM_INFO("Sending reply inside b2b dialog. call_id=%s | tag=%s | to_tag=%s | b2b_server_key=%s | entity=%d | method=%d | code=%d | readon=%s.\n",
         call_id, cli_fromtag, cli_dst_b2b_tag, b2b_server_key, et, method, code, text);

   int status = OK;

   b2b_rpl_data_t rpl_data;
   b2b_dlginfo_t dlginfo;
   memset(&rpl_data, 0, sizeof(b2b_rpl_data_t));
   memset(&dlginfo,0, sizeof(b2b_dlginfo_t));

   str dgl_callid;
   char dgl_callid_c[128]; bzero(dgl_callid_c,128);
   sprintf(dgl_callid_c,call_id);
   dgl_callid.s=dgl_callid_c;
   dgl_callid.len=strlen(dgl_callid_c);
   dlginfo.callid=dgl_callid;

   str dgl_ftag;
   char dgl_ftag_c[128]; bzero(dgl_ftag_c,128);
   sprintf(dgl_ftag_c,cli_fromtag);
   dgl_ftag.s=dgl_ftag_c;
   dgl_ftag.len=strlen(dgl_ftag_c);
   dlginfo.fromtag=dgl_ftag;

   str dgl_ttag;
   char dgl_ttag_c[128]; bzero(dgl_ttag_c,128);
   sprintf(dgl_ttag_c,cli_dst_b2b_tag);
   dgl_ttag.s=dgl_ttag_c;
   dgl_ttag.len=strlen(dgl_ttag_c);
   dlginfo.totag=dgl_ttag;

   str b2b_key;
   char b2b_key_c[128]; bzero(b2b_key_c,128);
   sprintf(b2b_key_c,b2b_server_key);
   b2b_key.s=b2b_key_c;
   b2b_key.len=strlen(b2b_key_c);

   rpl_data.et=et;
   rpl_data.b2b_key=&b2b_key;
   rpl_data.method =method;
   rpl_data.code =code;

   str reason;
   char reason_c[128];
   bzero(reason_c,128);
   sprintf(reason_c,text);
   reason.s=reason_c;
   reason.len=strlen(reason_c);

   rpl_data.text=&reason;
   rpl_data.dlginfo=&dlginfo;

   b2b_api.send_reply(&rpl_data);

   return status;
}

/******************************************************************************
 *        NAME: parse_inDialog_200OK
 * DESCRIPTION: This function is invoked by 'parse_200OK' and is used to parse a 200OK
 * 		response to an in dialog invite.
 *		Check state of connection and  invoke parsing functions.
 *****************************************************************************/

int
parse_inDialog_200OK(struct sip_msg *msg, char ** sdp_origin, conn * connection, client * cli)
{
   int status = OK;
   client * cli_dst = NULL;
   get_client(connection, cli, &cli_dst); // Get destination client to insert the chosen payload
   if (cli_dst == NULL)
   {
      LM_ERR("ERROR: No destination client encountered. b2bcallid=%d | conn_state=%d | call_id=%s | client_id=%d | state=%d | error_code=%d\n"
    		  ,connection->id,connection->s,connection->call_id,cli->id,cli->s,PARSER_ERROR);
      return PARSER_ERROR;
   }

   int original_state = cli->s; // Get original state for later in this function be assigned again

   if (cli->s != ON_HOLD)
      cli->s = TWO_OK; //If this client is on hold, connection ip is 0.0.0.0. If not, put TWO_OK (for parsing purposes only)

   //Check in the state of the clients

   if (cli_dst->s == TO_HOLD)
   {
      LM_NOTICE("Put call on hold. b2bcallid=%d | src_client_id=%d | dst_client_id=%d\n",connection->id,cli->id,cli_dst->id);
      LM_INFO("Put call on hold. conn_state=%d | src_client_state=%d | src_client_tag=%s | dst_client_state=%d | dst_client_tag=%s\n",
    		  connection->s,cli->s,cli->tag,cli_dst->s,cli_dst->tag);
      cli_dst->s = PENDING_HOLD;
      status = parse_sdp_b2b(msg, sdp_origin, connection, cli);
      cli->s = original_state; // Put back the original state
   }
   else if (cli_dst->s == OFF_HOLD)
   {
      LM_NOTICE("Put off hold.b2bcallid=%d | src_cli_id=%d | dst_cli_id=%d\n",connection->id,cli->id,cli_dst->id);
      LM_INFO("Put off hold. conn_state=%d | src_client_state=%d | src_client_tag=%s | dst_client_state=%d | dst_client_tag=%s\n",
    		  connection->s,cli->s,cli->tag,cli_dst->s,cli_dst->tag);
      cli_dst->s = PENDING_OFF_HOLD;
      status = parse_sdp_b2b(msg, sdp_origin, connection, cli);
      cli->s = original_state; // Put back the original state
   }
   else if (cli_dst->s == PENDING_INVITE && cli->s == EARLY_MEDIA_C)
   {
      LM_NOTICE("Process 200OK after early media.b2bcallid=%d | src_client_id=%d | dst_client_id=%d\n",connection->id,cli->id,cli_dst->id);
      LM_INFO("Process 200OK after early media. conn_state=%d | src_client_state=%d | src_client_tag=%s | dst_client_state=%d | dst_client_tag=%s\n",
    		  connection->s,cli->s,cli->tag,cli_dst->s,cli_dst->tag);
      status = parse_sdp_b2b(msg, sdp_origin, connection, cli);
      cli->s = PENDING_OFF_EARLY_MEDIA_C; // Put back the original state
   }
   else if (cli_dst->s == PENDING_INVITE)
   {
      LM_NOTICE("Parsing 200OK after reinvite in 200OK.b2bcallid=%d | src_client_id=%d | dst_client_id=%d\n",connection->id,cli->id,cli_dst->id);
      LM_INFO("Parsing 200OK after reinvite in 200OK. conn_state=%d | src_client_state=%d | src_client_tag=%s | dst_client_state=%d | dst_client_tag=%s\n",
    		  connection->s,cli->s,cli->tag,cli_dst->s,cli_dst->tag);
      status = parse_sdp_b2b(msg, sdp_origin, connection, cli);
      cli->s = PENDING_200OK;
   }
   else
   {
      LM_INFO("Nothing to do\n");
      return status;
   }

   if (status == VIDEO_UNSUPPORTED)
   {
      LM_INFO("Video is not supported, skipping the remaining of sdp lines\n");
      status = OK;
   }

   if (status != OK)
   {
      LM_ERR("ERROR: Error parsing sip message. b2bcallid=%d | conn_state=%d | call_id=%s | client_id=%d | state=%d | error_code=%d\n",
    		  connection->id,connection->s,connection->call_id,cli->id,cli->s,status);
      free_ports_client(cli_dst);
      free_ports_client(cli);
   }

   return status;
}

/******************************************************************************
 *        NAME: parse_183
 * DESCRIPTION: This function is invoked by when a message with status 183 arrives.
 * 				Identifies the connection that belongs to the call and invoke the parsing functions(parse_sdp_b2b).
 *****************************************************************************/

static int
parse_183(struct sip_msg *msg)
{
   LM_INFO("Parsing early media\n");
   char * message = get_body(msg); //msg->buf;
   int status = OK; //Holds the status of the operation
   int i = 0;
   int proceed = 1;
   conn * connection = NULL;
   client * cli = NULL;
   struct to_body *pfrom; //Structure contFrom header
   struct to_body *pTo;
   struct cseq_body * cseq;
   struct sip_uri * uri=NULL;
   struct hdr_field * user_agent=NULL;

   //Remove \r and \n from sip message

   char msg_copy[msg->len+2];
   int size=sizeof(msg_copy)/sizeof(char);
   if( size >= (msg->len+2) )
   {
	   snprintf(msg_copy,msg->len+1,msg->buf);
	   remove_newline_str(msg_copy);
	   LM_INFO("Received message : [%s]\n", msg_copy);
   }
   else
   {
	   LM_WARN("Failed to allocate space for 'msg_copy' variable\n");
   }




   if (parse_headers(msg, HDR_CSEQ_F | HDR_TO_F | HDR_FROM_F, 0) != 0)
   {
      LM_ERR("ERROR: Error parsing headers. error_code=%d | message %s\n",PARSER_ERROR,msg_copy);
      return PARSER_ERROR;
   }

   if (parse_from_header(msg) != 0)
   { // Parse header FROM
      LM_ERR("ERROR: Bad Header. error_code=%d | message %s\n",PARSER_ERROR,msg_copy);
      return PARSER_ERROR;
   }

   pfrom = get_from(msg); //Get structure containing From header
   pTo = get_to(msg);
   cseq = get_cseq(msg);
   uri = parse_to_uri(msg);

   //Separately parse User-Agent header because is not a mandatory header in a sip message
   if (parse_headers(msg, HDR_USERAGENT_F, 0) != 0)
   {
      LM_INFO("User-Agent header is not present in sip message\n");
      user_agent = msg->user_agent;
   }

   char from_tag[128];
   char to_tag[128];
   char * src_ipP = ip_addr2a(&msg->rcv.src_ip);
   char callID[128];
   char cseq_call[128];
   char src_ip[25];
   char cseq_copy[128];
   char cseq_conn_copy[128];
   char user_name[128];
   char user_agent_str[128];
   bzero(from_tag, 128);
   bzero(to_tag, 128);
   bzero(callID, 128);
   bzero(cseq_call, 64);
   bzero(src_ip, 25);
   bzero(cseq_copy, 128);
   bzero(cseq_conn_copy, 128);
   bzero(user_name,128);
   bzero(user_agent_str,128);
   sprintf(src_ip, src_ipP);
   sprintf(cseq_copy, cseq_call); // This string will be copied because strtok_r can change this string.
   if(uri!=NULL)
	   snprintf(user_name,uri->user.len+1,uri->user.s);
   if(user_agent!=NULL)
	   snprintf(user_agent_str,user_agent->body.len+1,user_agent->body.s);

   char * method; // Needed for strtok_r
   char * number;
   number = strtok_r(cseq_copy, " ", &method); // Divide a string into tokens.

   snprintf(callID, (msg->callid->body.len + 1), msg->callid->body.s); //Get callID
   snprintf(from_tag, pfrom->tag_value.len + 1, pfrom->tag_value.s);
   snprintf(to_tag, pTo->tag_value.len + 1, pTo->tag_value.s);
   sprintf(cseq_call, "%.*s %.*s", cseq->number.len, cseq->number.s, cseq->method.len, cseq->method.s);

   LM_INFO("Incoming 183 message. call_id=%s | src_ip=%s | from_tag=%s | to_tag=%s | cseq=%s | username=%s\n",
   		   callID,src_ip,from_tag,to_tag,cseq_call,user_name);

   for (i = 0; i < MAX_CONNECTIONS; i++) // Loop through the connections array
   {
      if (connections[i].s != TERMINATED && connections[i].s != EMPTY && connections[i].s != REFER_TO
            && ((strcmp(connections[i].call_id, callID) == 0) || (strcmp(connections[i].b2b_client_callID, callID) == 0)))
      {
         LM_INFO("Found connection. b2bcallid=%d | previous_cseq=%s | conn_state=%d\n", connections[i].id, connections[i].cseq, connections[i].s);
         sprintf(cseq_conn_copy, connections[i].cseq);

         char * method_previous; // Needed for strtok_r
         char * number_previous;
         number_previous = strtok_r(cseq_conn_copy, " ", &method_previous); // Divide a string into tokens.

         if ((strcmp(method_previous, "INVITE") == 0) && connections[i].s == PENDING)
         {
            LM_INFO("Creating client for early media\n");
            int c = 0;
            for (c = 0; c < MAX_CLIENTS && proceed == 1; c++)
            { //Find an empty client
               if (connections[i].clients[c].is_empty == 0)
               {
                  LM_INFO("Found empty client. b2bcallid=%d | client_id%d\n", connections[i].id,c);
                  char * caller_src_ip = connections[i].clients[0].src_ip; //Retrieve caller source IP

                  cli = &(connections[i].clients[c]);
                  clean_client(cli);
                  cli->is_empty = 1;
                  connection = &(connections[i]);


                  char id[4];

                  sprintf(id, "%d%d", i, c); // Create id based on connections and clients indexes
                  cli->id = atoi(id);
                  sprintf(connection->cseq, cseq_call);
                  sprintf(cli->src_ip, src_ip);
                  sprintf(cli->user_name,user_name);
                  sprintf(connection->clients[0].user_agent,user_agent_str);
                  sprintf(cli->dst_ip, caller_src_ip);
                  sprintf(cli->b2b_tag, connections[i].b2b_client_serverID); // Put b2b_client_serverID, this key will be used by opensips to identify second client.
                  sprintf(cli->tag, to_tag);
                  cli->s = PENDING_EARLY_MEDIA_C;
                  connection->s = PENDING_EARLY_MEDIA;
                  proceed = 0;
                  LM_NOTICE("Matched client to call.b2bcallid=%d | client_id=%d | src_ip=%s | tag=%s | state=%d\n",
                		  connections[i].id,cli->id, cli->src_ip, cli->tag, cli->s);
               }
            }
         }
         else
         {
            LM_ERR("ERROR.This message requires a previous INVITE. b2bcallid=%d | message_call_id=%s | src_ip=%s | conn_call_id=%s | conn_state %d | conn_cseq=%s | error_code=%d\n",
            		connections[i].id, callID, src_ip, connections[i].call_id, connections[i].s, connections[i].cseq,PARSER_ERROR);
            return PARSER_ERROR;
         }
      }
   }

   if (cli == NULL || connection == NULL)
   {
      LM_ERR("ERROR. No connection was found. ip=%s | call_id=%s | from_tag=%s | to_tag=%s | error_code=%d\n", src_ip, callID,from_tag,to_tag,NOT_FOUND);
      return NOT_FOUND;
   }

   status = parse_sdp_b2b(msg, &message, connection, cli);

   if (status != OK)
   {
      LM_ERR("ERROR: Error parsing sip message. b2bcallid=%d | conn_state=%d | call id %s | client_id=%d | client state=%d | error_code=%d | message [%s]\n",
    		  connection->id,connection->s,connection->call_id,cli->id,cli->s,status,msg_copy);
      switch (status)
      {
         case XCODER_CMD_ERROR:
        	LM_ERR("ERROR. Xcoder command error. b2bcallid=%d | error_code=%d | reason=%s\n",connection->id,SERVER_INTERNAL_ERROR,"Server Internal Error");
            cancel_connection(connection);
            return SERVER_INTERNAL_ERROR;
            break;
         case XCODER_TIMEOUT:
        	LM_ERR("ERROR.Xcoder timeout. b2bcallid=%d | error_code=%d | reason=%s\n",connection->id,SERVER_TIME_OUT,"Server Time-out");
            cancel_connection(connection);
            return SERVER_TIME_OUT;
            break;
         case NOT_ACCEPTABLE_HERE:
        	LM_ERR("ERROR. Media type is not supported. b2bcallid=%d | error_code=%d | reason=%s\n",connection->id,NOT_ACCEPTABLE_HERE,"Not Acceptable here");
            cancel_connection(connection);
            return NOT_ACCEPTABLE_HERE;
            break;
         default:
        	LM_ERR("ERROR. Service is full. b2bcallid=%d | error_code=%d | reason=%s\n",connection->id,SERVER_INTERNAL_ERROR,"Server Internal Error");
            cancel_connection(connection);
            return SERVER_INTERNAL_ERROR;
            break;
      }
   }

   LM_INFO("Start to create early media call\n");
   client * caller = NULL;
   get_client(connection, cli, &caller); //Get destination client
   if (caller == NULL)
   {
      LM_ERR("Error: Error retrieving destination client. b2bcallid=%d | conn_state=%d | call_id=%s | client_id=%d | client state=%d | error_code=%d\n",
    		  connection->id,connection->s,connection->call_id,cli->id,cli->s,GENERAL_ERROR);

      send_remove_to_xcoder(connection);
      cancel_connection(connection);
      return GENERAL_ERROR;
   }

   status = create_call(connection, caller); // Send create command to xcoder

   if (status != OK)
   {
      LM_ERR("ERROR: Error creating early media call. b2bcallid=%d | conn_state=%d | call_id=%s | client_id=%d | state=%d | error_code=%d\n",
    		  connection->id,connection->s,connection->call_id,cli->id,cli->s,status);

      send_remove_to_xcoder(connection);
      switch (status)
      {
         case XCODER_CMD_ERROR:
        	LM_ERR("ERROR. Xcoder command error. b2bcallid=%d | error_code=%d | reason=%s\n",connection->id,SERVER_INTERNAL_ERROR,"Server Internal Error");
            cancel_connection(connection);
            return SERVER_INTERNAL_ERROR;
            break;
         case XCODER_TIMEOUT:
        	LM_ERR("ERROR.Xcoder timeout. b2bcallid=%d | error_code=%d | reason=%s\n",connection->id,SERVER_TIME_OUT,"Server Time-out");
            cancel_connection(connection);
            return SERVER_TIME_OUT;
            break;
         case NOT_ACCEPTABLE_HERE:
        	LM_ERR("ERROR. Media type is not supported. b2bcallid=%d | error_code=%d | reason=%s\n",connection->id,NOT_ACCEPTABLE_HERE,"Not Acceptable here");
            cancel_connection(connection);
            return NOT_ACCEPTABLE_HERE;
            break;
         default:
        	LM_ERR("ERROR. Error creating call. b2bcallid=%d | error_code=%d | reason=%s\n",connection->id,SERVER_INTERNAL_ERROR,"Server Internal Error");
            cancel_connection(connection);
            return SERVER_INTERNAL_ERROR;
            break;
      }
   }

   cli->s = EARLY_MEDIA_C;
   connection->s = EARLY_MEDIA;

   return OK;

}

/******************************************************************************
 *        NAME: parse_200OK
 * DESCRIPTION: This function is invoked in opensips routine when a 200OK message arrives.
 * 		Invoke the necessary parse functions that will perform parsing and change sdp content.
 *****************************************************************************/

static int
parse_200OK(struct sip_msg *msg)
{
   LM_INFO("Parse 200OK\n");

   /*typedef struct b2b_rpl_data
    {
    enum b2b_entity_type et;
    str* b2b_key;
    int method;
    int code;
    str* text;
    str* body;
    str* extra_headers;
    b2b_dlginfo_t* dlginfo;
    }b2b_rpl_data_t;*/

   //rpl_data.method =METHOD_BYE;
   char * message = get_body(msg); //msg->buf;
   int status = OK; //Holds the status of the operation
   int i = 0;
   conn * connection = NULL;
   client * cli = NULL;
   client * cli_dst = NULL;
   int proceed = 1; //If a client is found and a open space exists, proceed is set to 0
   struct to_body *pfrom; //Structure contFrom header
   struct to_body *pTo;
   struct cseq_body * cseq;
   struct sip_uri * uri=NULL;
   struct hdr_field * user_agent=NULL;

   if (parse_headers(msg, HDR_CSEQ_F | HDR_TO_F | HDR_FROM_F, 0) != 0)
   {
	   LM_ERR("ERROR: Error parsing headers. error_code=%d\n",PARSER_ERROR);
	   return PARSER_ERROR;
   }

   if (parse_from_header(msg) != 0)
   { // Parse header FROM
      LM_ERR("ERROR: Bad Header. error_code=%d\n",PARSER_ERROR);
      return PARSER_ERROR;
   }

   pfrom = get_from(msg); //Get structure containing From header
   pTo = get_to(msg);
   cseq = get_cseq(msg);
   uri = parse_to_uri(msg);

   //Remove \r and \n from sip message

   char msg_copy[msg->len+2];
   int size=sizeof(msg_copy)/sizeof(char);
   if( size >= (msg->len+2) )
   {
	   snprintf(msg_copy,msg->len+1,msg->buf);
	   remove_newline_str(msg_copy);
	   LM_INFO("Message received: [%s]\n", msg_copy);
   }
   else
   {
	   LM_WARN("Failed to allocate space for 'msg_copy' variable\n");
   }

   //Separately parse User-Agent header because is not a mandatory header in a sip message
   if (parse_headers(msg, HDR_USERAGENT_F, 0) != 0)
   {
      LM_INFO("User-Agent header is not present in sip message\n");
      user_agent = msg->user_agent;
   }

   char from_tag[128];
   char to_tag[128];
   char src_ip[25];
   char callID[128];
   char cseq_call[128];
   char * src_ipP = ip_addr2a(&msg->rcv.src_ip);
   char user_name[128];
   char user_agent_str[128];
   bzero(from_tag, 128);
   bzero(to_tag, 128);
   bzero(src_ip, 25);
   bzero(callID, 128);
   bzero(cseq_call, 64);
   bzero(user_name,128);
   bzero(user_agent_str,128);
   sprintf(src_ip, src_ipP);
   snprintf(callID, (msg->callid->body.len + 1), msg->callid->body.s); //Get callID
   snprintf(from_tag, pfrom->tag_value.len + 1, pfrom->tag_value.s);
   snprintf(to_tag, pTo->tag_value.len + 1, pTo->tag_value.s);
   sprintf(cseq_call, "%.*s %.*s", cseq->number.len, cseq->number.s, cseq->method.len, cseq->method.s);

   if(uri!=NULL)
	   snprintf(user_name,uri->user.len+1,uri->user.s);
   if(user_agent!=NULL)
	   snprintf(user_agent_str,user_agent->body.len+1,user_agent->body.s);

   LM_INFO("Parse 200OK. call_id=%s | src_ip=%s | from_tag=%s | to_tag=%s | cseq=%s | username=%s\n",
		   callID,src_ip,from_tag,to_tag,cseq_call,user_name);

   char copy[64];
   bzero(copy, 64);
   sprintf(copy, cseq_call);
   char * method; // Needed for strtok_r
   char * number;
   number = strtok_r(copy, " ", &method); // Divide a string into tokens.

   ////////////////////////////////////// Check if this ip is present in more than one connection ///////////////////////////

   int match_count = 0; //Count the number of matches for this ip
   for (i = 0; i < MAX_CONNECTIONS; i++) // Loop through the connections array
   {
      if (connections[i].s != TERMINATED && connections[i].s != EMPTY && connections[i].s != REFER_TO && connections[i].s != PENDING)
      {
         int c = 0;
         for (c = 0; c < MAX_CLIENTS; c++) // Loop through clients array within a connection
         {
            if (connections[i].clients[c].is_empty == 1 && (strcmp(connections[i].clients[c].src_ip, src_ip) == 0)) //Check if a client know this ip in any way
            {
               if ((strcmp(connections[i].b2b_client_callID, callID) == 0) || (strcmp(connections[i].call_id, callID) == 0)) //Check if is a duplicate 200OK
               {

                  char second_copy[64];
                  bzero(second_copy, 64);
                  sprintf(second_copy, connections[i].cseq);
                  char * method_previous; // Needed for strtok_r
                  char * number_previous;
                  number_previous = strtok_r(second_copy, " ", &method_previous); // Divide a string into tokens.

                  cli_dst = NULL;
                  get_client(&(connections[i]), &(connections[i].clients[c]), &cli_dst); //Get destination client
                  if (cli_dst == NULL)
                  {
                	  LM_ERR("Error: Null destination client. b2bcallid=%d | conn_state=%d | call_id=%s | client_id=%d | client_state=%d | username=%s | src_ip=%s | tag=%s | error_code=%d\n",
                			  connections[i].id,connections[i].s,connections[i].call_id,connections[i].clients[c].id,connections[i].clients[c].s,connections[i].clients[c].user_name, connections[i].clients[c].src_ip, connections[i].clients[c].tag, GENERAL_ERROR);
                	  send_remove_to_xcoder(&(connections[i]));
                	  cancel_connection(&(connections[i]));
                	  return GENERAL_ERROR;
                  }

                  //Check if is a response to a Call on hold invite or duplicate message
                  cli = &(connections[i].clients[c]);
                  if (strcmp(method_previous, "INVITE") != 0 && cli_dst->s != TO_HOLD && cli_dst->s != OFF_HOLD && cli->s != PENDING_INVITE)
                  {
                     LM_NOTICE("Wrong message flow, this response requires a previous invite request. b2bcallid=%d | conn_state=%d | call_id=%s | client_id=%d | client_state=%d | username=%s | src_ip=%s | tag=%s | dst_cli_id=%d | dst_cli_state=%d | dst_cli_username=%s | dst_client_ip=%s | dst_client_tag=%s\n",
                    		 connections[i].id,connections[i].s,connections[i].call_id,cli->id,cli->s,cli->user_name, cli->src_ip, cli->tag, cli_dst->id,cli_dst->s,cli_dst->user_name, cli_dst->src_ip, cli_dst->tag);
                     return TO_DROP;
                  }
                  else if ((cli_dst->s == TO_HOLD || cli_dst->s == OFF_HOLD) && (strcmp(cli->tag, to_tag) == 0))
                  {
                     LM_NOTICE("Parse in dialog 200OK. Putting call on/off hold. b2bcallid=%d | client_id=%d",connections[i].id, cli->id);
                     LM_INFO("Parse in dialog 200OK.Putting call on/off hold. b2bcallid=%d | client_id=%d | username=%s | cli_state=%d | cli_dst_state=%d\n",
                    		 connections[i].id, cli->id,cli->user_name, cli->s, cli_dst->s);
                     status = parse_inDialog_200OK(msg, &message, &(connections[i]), cli);
                     if(status!=OK)
                     {
                        LM_ERR("ERROR: Error parsing 200OK sip message. b2bcallid=%d | client_id=%d | username=%s | cli_state=%d | cli_dst_state=%d\n",
                              connections[i].id, cli->id,cli->user_name, cli->s, cli_dst->s);
                        cancel_connection(&(connections[i]));
                     }
                     return status;
                  }
                  else if (connections[i].s == REINVITE && cli_dst->s == PENDING_INVITE)
                  {
                     LM_NOTICE("Parse in dialog 200OK. b2bcallid=%d | client_id=%d | username=%s\n",
                    		 connections[i].id, cli->id,cli->user_name);
                     status = parse_inDialog_200OK(msg, &message, &(connections[i]), cli);
                     if(status!=OK)
                     {
                        LM_ERR("ERROR: Error parsing 200OK sip message. b2bcallid=%d | client_id=%d | username=%s | cli_state=%d | cli_dst_state=%d\n",
                              connections[i].id, cli->id,cli->user_name, cli->s, cli_dst->s);
                        cancel_connection(&(connections[i]));
                     }
                     return status;

                  }
                  else if (connections[i].s == EARLY_MEDIA && cli_dst->s == PENDING_INVITE && (strcmp(cli->tag, to_tag) == 0))
                  {
                     LM_NOTICE("Parse 200OK after early media. b2bcallid=%d | client_id=%d | username=%s\n",
                    		 connections[i].id, cli->id,cli->user_name);
                     status = parse_inDialog_200OK(msg, &message, &(connections[i]), cli);
                     if(status!=OK)
                     {
                        LM_ERR("ERROR: Error parsing 200OK sip message. b2bcallid=%d | client_id=%d | username=%s | cli_state=%d | cli_dst_state=%d\n",
                              connections[i].id, cli->id,cli->user_name, cli->s, cli_dst->s);
                        cancel_connection(&(connections[i]));
                     }
                     return status;
                  }

               }
               // commented for testing purposes, uncomment and check if is required.
               /*else{
                LM_ERR("ERROR: Client found with different call-ID. Connection id = %d\n",connections[i].id);
                send_remove_to_xcoder(&(connections[i]));
                cancel_connection(&(connections[i]));
                clean_connection(&(connections[i]));
                return INIT_ERROR;
                }*/
               match_count++;
            }
         }
      }
   }

   if (match_count > 0)
   {
      LM_INFO("This ip [%s], is already active in one connection. b2bcallid=%d\n",
    		  src_ip,connections[i].id);
   }

   ////////////////////////////////////// Find the connection and create a new client ///////////////////////////

   for (i = 0; i < MAX_CONNECTIONS && proceed == 1; i++) //Loop to find right connection
   {
      if ((connections[i].s == PENDING || connections[i].s == REINVITE) && (strcmp(connections[i].b2b_client_callID, callID) == 0)) //Compare with ID generated by B2B
      {
         LM_INFO("Found connection. b2bcallid=%d | conn_state=%d | call_id=%s | b2b_gen_call id=%s\n",connections[i].id, connections[i].s,connections[i].call_id, connections[i].b2b_client_callID);
         sprintf(connections[i].clients[0].dst_ip, src_ip); //Set destination ip for caller

         cli_dst = &(connections[i].clients[0]);
         char * caller_src_ip = cli_dst->src_ip; //Retrieve caller source IP

         int c = 0;
         for (c = 0; c < MAX_CLIENTS && proceed == 1; c++)
         { //Find an empty client
            if (connections[i].clients[c].is_empty == 0)
            {
               LM_INFO("Found empty slot in position %d. b2bcallid=%d\n", c,connections[i].id);

               connection = &(connections[i]);
               cli = &(connections[i].clients[c]);
               clean_client(cli);

               char id[4];

               sprintf(id, "%d%d", i, c); // Create id based on connections and clients indexes
               cli->id = atoi(id);
               sprintf(connection->cseq, cseq_call);
               sprintf(cli->src_ip, src_ip);
               sprintf(cli->user_name,user_name);
               sprintf(connection->clients[0].user_agent,user_agent_str);
               sprintf(cli->dst_ip, caller_src_ip);
               sprintf(cli->b2b_tag, connections[i].b2b_client_serverID); // Put b2b_client_serverID, this key will be used by opensips to identify second client.
               sprintf(cli->tag, to_tag);
               cli->is_empty = 1;
               cli->s = TWO_OK;

               proceed = 0;
               LM_NOTICE("Matched client to call: b2bcallid=%d | client_id=%d | username=%s | src_ip=%s | tag=%s\n", connection->id, cli->id, cli->user_name, cli->src_ip, cli->tag);
            }
         }
      }
   }

   if (cli == NULL || proceed == 1)
   {
	   LM_ERR("ERROR:No client was found. ip=%s | call_id=%s | error_code=%d\n", src_ip, callID,NOT_FOUND);
	   return NOT_FOUND;
   }

   status = parse_sdp_b2b(msg, &message, connection, cli);

   //Currently video is not supported but is bypassed
   if (status == VIDEO_UNSUPPORTED)
   {
      status = OK;
   }

   if (status != OK)
   {
      LM_ERR("ERROR: Error parsing sip message. b2bcallid=id %d | conn_state=%d | call_id=%s | client_id=%d | client state=%d\n",
    		  connection->id,connection->s,connection->call_id,cli->id,cli->s);

      send_remove_to_xcoder(connection); //Xcoder will free ports for both clients.
      //Send error reply to caller
      cli_dst = NULL;
      get_client(connection, cli, &cli_dst); //Get destination client
      if (cli_dst == NULL)
      {
        LM_ERR("Error: Null destination client. Cannot send error reply. b2bcallid=%d | conn_state=%d | call_id=%s | client_id=%d | client_state=%d | username=%s | src_ip=%s | tag=%s | error_code=%d\n",
              connection->id,connection->s,callID,cli->id,cli->s,cli->user_name, cli->src_ip, cli->tag, GENERAL_ERROR);
      }
      if(strcmp(connection->call_id, callID)==0)
         send_reply_b2b(callID, cli_dst->b2b_tag,cli->tag, connection->b2b_client_serverID, B2B_CLIENT, METHOD_INVITE, 606, "Not Acceptable");
      else if(strcmp(connection->b2b_client_callID, callID) == 0)
         send_reply_b2b(connection->call_id, cli_dst->tag,cli->b2b_tag, connection->b2b_client_serverID, B2B_SERVER, METHOD_INVITE, 606, "Not Acceptable");
      else
         LM_ERR("Can not math callid to send error reply. b2bcallid=%d | conn_state=%d | call_id=%s | client_id=%d | client_state=%d | username=%s | src_ip=%s | tag=%s | error_code=%d\n",
               connection->id,connection->s,callID,cli->id,cli->s,cli->user_name, cli->src_ip, cli->tag, GENERAL_ERROR);

      switch (status)
      {
         case XCODER_CMD_ERROR:
        	LM_ERR("ERROR. Xcoder command error. b2bcallid=%d | error_code=%d | reason=%s\n",connection->id,SERVER_INTERNAL_ERROR,"Server Internal Error");
            cancel_connection(connection);
            return SERVER_INTERNAL_ERROR;
            break;
         case XCODER_TIMEOUT:
        	LM_ERR("ERROR.Xcoder timeout. b2bcallid=%d | error_code=%d | reason=%s\n",connection->id,SERVER_TIME_OUT,"Server Time-out");
            cancel_connection(connection);
            return SERVER_TIME_OUT;
            break;
         case NOT_ACCEPTABLE_HERE:
        	LM_ERR("ERROR. Media type is not supported. b2bcallid=%d | error_code=%d | reason=%s\n",connection->id,NOT_ACCEPTABLE_HERE,"Not Acceptable here");
            cancel_connection(connection);
            return NOT_ACCEPTABLE_HERE;
            break;
         case SERVER_UNAVAILABLE:
			LM_ERR("ERROR. Media type is not supported. b2bcallid=%d | error_code=%d | reason=%s\n",connection->id,SERVER_UNAVAILABLE,"Service Unavailable");
			cancel_connection(connection);
			return SERVER_UNAVAILABLE;
			break;
         default:
            cancel_connection(connection);
            LM_ERR("ERROR. Service is full. b2bcallid=%d | error_code=%d | reason=%s\n",connection->id,SERVER_INTERNAL_ERROR,"Server Internal Error");
            return SERVER_INTERNAL_ERROR;
            break;
      }
   }

   cli->s = PENDING_200OK;
   connection->s = CREATING;

   return OK;
}


/******************************************************************************
 *        NAME: parse_inDialog_invite
 * DESCRIPTION: This function is invoked when an in dialog invite arrives.
 *		Check state of connection and invoke parsing function with the necessary parameters.
 *****************************************************************************/

static int
parse_inDialog_invite(struct sip_msg *msg)
{
   LM_INFO("Parse invite. Check if is a generated invite or an indialog invite\n");
   char * message = get_body(msg);

   //Remove \r and \n from sip message
   char msg_copy[msg->len+2];
   int size=sizeof(msg_copy)/sizeof(char);
   if( size >= (msg->len+2) )
   {
	   snprintf(msg_copy,msg->len+1,msg->buf);
	   remove_newline_str(msg_copy);
	   LM_INFO("Message : [%s]\n", msg_copy);
   }
   else
   {
	   LM_WARN("Failed to allocate space for 'msg_copy' variable\n");
   }

   conn * connection = NULL;
   client * cli_dst = NULL;
   client * cli = NULL;
   int status = OK;
   int conn_original_state = ACTIVE;
   int caller_original_state = ACTIVE_C;
   int callee_original_state = ACTIVE_C;

   if (parse_sdp(msg) < 0)
   {
      LM_ERR("Unable to parse sdp. error_code=%d | message %s\n",PARSER_ERROR,msg_copy);
      clean_connection_v2(msg);

      return PARSER_ERROR;
   }

   struct sdp_info* sdp;
   sdp = msg->sdp;

   struct sdp_session_cell *sessions;
   sessions = sdp->sessions;
//
//   struct sdp_stream_cell* streams;
//   streams = sessions->streams;
//
//   struct sdp_payload_attr *payload_attr;
//   payload_attr = streams->payload_attr;

   //print_sdp(sdp,3);
   /*while(streams!=NULL)
    {
    LM_NOTICE("payloads : %.*s\n",streams->payloads.len,streams->payloads.s);
    LM_NOTICE("port : %.*s\n",streams->port.len,streams->port.s);
    LM_NOTICE("sendrecv_mode : %.*s\n",streams->sendrecv_mode.len,streams->sendrecv_mode.s);

    streams = streams->next;
    }*/

   struct to_body *pfrom; //Structure From header
   struct to_body *pTo; //Structure To header
   struct cseq_body * cseq; //Structure CSEQ header
   struct sip_uri * uri=NULL;

   if (parse_headers(msg, HDR_CSEQ_F | HDR_TO_F | HDR_FROM_F, 0) != 0)
   {
      LM_ERR("ERROR: Error parsing headers. error_code=%d | message %s\n",PARSER_ERROR,msg_copy);
      return PARSER_ERROR;
   }

   if (parse_from_header(msg) != 0) // Parse header FROM
   {
      LM_ERR("ERROR: Bad From header. error_code=%d | message %s\n",PARSER_ERROR,msg_copy);
      clean_connection_v2(msg);
      return PARSER_ERROR;
   }
   pfrom = get_from(msg); //Get structure containing From header
   pTo = get_to(msg);
   cseq = get_cseq(msg);
   uri = parse_from_uri(msg);

   char callID[128];
   char conn_ip[25];
   char from_tag[128];
   char to_tag[128];
   char cseq_call[128];
   bzero(callID, 128);
   bzero(conn_ip, 25);
   bzero(from_tag, 128);
   bzero(to_tag, 128);
   bzero(cseq_call, 64);

   snprintf(from_tag, pfrom->tag_value.len + 1, pfrom->tag_value.s);
   snprintf(to_tag, pTo->tag_value.len + 1, pTo->tag_value.s);
   snprintf(conn_ip, sessions->ip_addr.len + 1, sessions->ip_addr.s);
   snprintf(callID, (msg->callid->body.len + 1), msg->callid->body.s);
   sprintf(cseq_call, "%.*s %.*s", cseq->number.len, cseq->number.s, cseq->method.len, cseq->method.s);

   LM_NOTICE("Check if is an in dialog invite. call_id=%s | src_ip=%s | from_tag=%s | to_tag=%s | user_name=%.*s\n",
   		   callID,conn_ip,from_tag, to_tag,pfrom->parsed_uri.user.len,pfrom->parsed_uri.user.s);

   int exit = 0;
   int i = 0;
   for (i = 0; i < MAX_CONNECTIONS && exit == 0; i++)
   {
      if (((connections[i].s != EMPTY) && (connections[i].s != TERMINATED))
            && ((strcmp(connections[i].call_id, callID) == 0) || (strcmp(connections[i].b2b_client_callID, callID) == 0))) //Find an empty connection
      {
         LM_INFO("Connection found. b2bcallid=%d | call_id=%s | conn_state=%d\n", connections[i].id, connections[i].call_id,connections[i].s);
         int c = 0;
         for (c = 0; c < MAX_CLIENTS && exit == 0; c++) // Checks if all clients are empty
         {
            LM_INFO("Check client. b2bcallid=%d | client_id=%d | tag=%s | b2b_tag=%s | state=%d | src_ip=%s | username=%s\n",
            		connections[i].id,connections[i].clients[c].id, connections[i].clients[c].tag, connections[i].clients[c].b2b_tag, connections[i].clients[c].s, connections[i].clients[c].src_ip, connections[i].clients[c].user_name);

            if ((connections[i].s == ACTIVE || connections[i].s == REINVITE) && (strcmp(connections[i].clients[c].b2b_tag, from_tag) == 0)) //This is a b2b generated invite
            {
               LM_INFO("Found client. b2bcallid=%d | client_id=%d | username=%s\n",connections[i].id,connections[i].clients[c].id,connections[i].clients[c].user_name);
               cli = &(connections[i].clients[c]);
               get_client(&(connections[i]), cli, &cli_dst); //Get destination client
               if (cli_dst == NULL)
               {
                  LM_ERR("Error: Null destination client. b2bcallid=%d | conn_state=%d | call_id=%s | src_client_id=%d | tag=%s | b2b_tag=%s | state=%d | src_ip=%s | username=%s | error_code=%d\n",
                		  connections[i].id,connections[i].s,connections[i].call_id,cli->id,cli->tag, cli->b2b_tag, cli->s, cli->src_ip, cli->user_name,GENERAL_ERROR);
                  return GENERAL_ERROR;
               }
               /*t_newtrans()
                t_reply
                t_reply("302","Moved Temporarily");
                */

               LM_INFO("Found destination client. b2bcallid=%d | client_id=%d | tag=%s | b2b_tag=%s | state=%d | src_ip=%s | username=%s\n",
            		   connections[i].id,cli_dst->id, cli_dst->tag, cli_dst->b2b_tag, cli_dst->s, cli_dst->src_ip, cli_dst->user_name);
               LM_INFO("Found destination client. b2bcallid=%d | client_id=%d | username=%s\n",connections[i].id,cli_dst->id,cli_dst->user_name);

               callee_original_state = cli_dst->s;
               caller_original_state = cli->s;

               connections[i].s = REINVITE;
               if ((strcmp(conn_ip, "0.0.0.0") == 0))
               {
                  LM_NOTICE("Call to hold. b2bcallid=%d | conn_state=%d | call_id=%s | client_id=%d | client_state=%d | username=%s\n",
                		  connections[i].id,connections[i].s,connections[i].call_id,connections[i].clients[c].id,connections[i].clients[c].s,connections[i].clients[c].user_name);
                  connection = &(connections[i]);
                  sprintf(connection->cseq, cseq_call);
                  cli->s = TO_HOLD;
                  sprintf(cli_dst->tag, to_tag);
                  status = parse_sdp_b2b(msg, &message, connection, &(connection->clients[c]));
                  exit = 1;
               }
               else if (cli->s == ON_HOLD)
               {
                  LM_NOTICE("Putting call off hold. b2bcallid=%d | conn_state=%d | call_id=%s | client_id=%d | client_state=%d | username=%s\n",
                		  connections[i].id,connections[i].s,connections[i].call_id,connections[i].clients[c].id,connections[i].clients[c].s,connections[i].clients[c].user_name);
                  connection = &(connections[i]);
                  sprintf(connection->cseq, cseq_call);
                  cli->s = OFF_HOLD;
                  sprintf(cli_dst->tag, to_tag);
                  status = parse_sdp_b2b(msg, &message, connection, &(connection->clients[c]));
                  exit = 1;
               }
               else
               {
                  LM_NOTICE("Parse in dialog invite. b2bcallid=%d | conn_state=%d | call_id=%s | client_id=%d | client_state=%d | username=%s\n",
                		  connections[i].id,connections[i].s,connections[i].call_id,connections[i].clients[c].id,connections[i].clients[c].s,connections[i].clients[c].user_name);
                  connection = &(connections[i]);
                  cli->s = INVITE_C;
                  sprintf(connection->cseq, cseq_call);
                  status = parse_sdp_b2b(msg, &message, connection, cli);
                  cli->s = PENDING_INVITE;
                  exit = 1;
               }
            }
         }
      }
   }

   if (connection == NULL || cli_dst==NULL || cli==NULL)
   {
      LM_INFO("Nothing to do. call_id=%s | user_name=%.*s\n",callID,pfrom->parsed_uri.user.len,pfrom->parsed_uri.user.s);
      return OK;
   }

   if (status == VIDEO_UNSUPPORTED)
      status = OK;

   if (status != OK)
   {

	  LM_ERR("ERROR: Error parsing sip message. b2bcallid=%d | conn_state=%d | call_id=%s | client_id=%d | state=%d | src_ip=%s | username=%s | tag=%s | b2b_tag=%s | error_code=%d | message [%s]\n",
    		  connections->id,connections->s,connections->call_id,cli->id,cli->s,cli->src_ip,cli->user_name,cli->tag,cli->b2b_tag,status, msg_copy);
      free_ports_client(cli);
      connection->s=conn_original_state;
      cli->s = caller_original_state;
      cli_dst->s = callee_original_state;
      switch (status)
      {
         case XCODER_CMD_ERROR:
        	LM_ERR("ERROR. Xcoder command error. b2bcallid=%d | error_code=%d | reason=%s\n",connections->id,SERVER_INTERNAL_ERROR,"Server Internal Error");
            return SERVER_INTERNAL_ERROR;
         case XCODER_TIMEOUT:
        	LM_ERR("ERROR.Xcoder timeout. b2bcallid=%d | error_code=%d | reason=%s\n",connections->id,SERVER_TIME_OUT,"Server Time-out");
            return SERVER_TIME_OUT;
         case NOT_ACCEPTABLE_HERE:
        	LM_ERR("ERROR. Media type is not supported. b2bcallid=%d | error_code=%d | reason=%s\n",connections->id,NOT_ACCEPTABLE_HERE,"Not Acceptable here");
            return NOT_ACCEPTABLE_HERE;
         case SERVER_UNAVAILABLE :
        	LM_ERR("ERROR. Service is full. b2bcallid=%d | error_code=%d | reason=%s\n",connections->id,SERVER_UNAVAILABLE,"Service Unavailable");
        	return SERVER_UNAVAILABLE;
         default:
        	LM_ERR("ERROR. Error parsing sip message. b2bcallid=%d | error_code=%d | reason=%s\n",connections->id,SERVER_INTERNAL_ERROR,"Server Internal Error");
            return SERVER_INTERNAL_ERROR;
            break;
      }
   }

   return status;
}

/******************************************************************************
 *        NAME: parse_invite
 * DESCRIPTION: Invokes parsing functions with the necessary parameters that will parse and
 *		manipulate the sdp content.
 *****************************************************************************/

static int
parse_invite(struct sip_msg *msg)
{
   LM_INFO("Parsing invite\n");

   char * message = get_body(msg); //Retrieves the body section of the sip message
   int i = 0;
   conn * connection = NULL;
   int status = OK;
   struct to_body *pfrom=NULL; //Structure From header
   struct cseq_body * cseq=NULL;
   struct sip_uri * from_uri=NULL;
   struct sip_uri * to_uri=NULL;
   struct hdr_field * user_agent=NULL;

   //Remove \r and \n from sip message

   char msg_copy[msg->len+2];
   int size=sizeof(msg_copy)/sizeof(char);
   if( size >= (msg->len+2) )
   {
	   snprintf(msg_copy,msg->len+1,msg->buf);
	   remove_newline_str(msg_copy);
	   LM_INFO("Received message : [%s]\n", msg_copy);
   }
   else
   {
	   LM_WARN("Failed to allocate space for 'msg_copy' variable\n");
   }


   if (parse_headers(msg, HDR_CSEQ_F | HDR_TO_F | HDR_FROM_F, 0) != 0)
   {
      LM_ERR("ERROR: Error parsing headers. error_code=%d\n",PARSE_ERROR);
      return PARSE_ERROR;
   }

   // Parse header FROM
   if (parse_from_header(msg) != 0)
   {
      LM_ERR("ERROR: Bad From header. error_code=%d\n",PARSE_ERROR);
      return PARSE_ERROR;
   }

   //Separately parse User-Agent header because is not a mandatory header in a sip message
   if (parse_headers(msg, HDR_USERAGENT_F, 0) != 0)
   {
      LM_INFO("User-Agent header is not present in sip message\n");
      user_agent = msg->user_agent;
   }

   pfrom = get_from(msg); //Get structure containing parsed From header
   cseq = get_cseq(msg);  //Get structure containing parsed Cseq header
   from_uri = parse_from_uri(msg);
   to_uri = parse_to_uri(msg);

   char * src_ip_from_msg = ip_addr2a(&msg->rcv.src_ip);
   char src_ip[25];
   char callID[128];
   char tag[128];
   char cseq_call[128];
   char user_name[128];
   char user_agent_str[128];
   char dst_host[32];
   char dst_user_name[128];

   bzero(src_ip, 25);
   bzero(callID, 128);
   bzero(tag, 128);
   bzero(cseq_call, 64);
   bzero(user_name,128);
   bzero(user_agent_str,128);
   bzero(dst_host,32);
   bzero(dst_user_name,128);
   sprintf(src_ip, src_ip_from_msg);
   snprintf(callID, (msg->callid->body.len + 1), msg->callid->body.s);
   snprintf(tag, pfrom->tag_value.len + 1, pfrom->tag_value.s);
   sprintf(cseq_call, "%.*s %.*s", cseq->number.len, cseq->number.s, cseq->method.len, cseq->method.s);

   if(from_uri!=NULL)
	   snprintf(user_name,from_uri->user.len+1,from_uri->user.s);
   if(user_agent!=NULL)
	   snprintf(user_agent_str,user_agent->body.len+1,user_agent->body.s);
   if(to_uri!=NULL)
   {
	   snprintf(dst_host,to_uri->host.len+1,to_uri->host.s);
	   snprintf(dst_user_name,to_uri->user.len+1,to_uri->user.s);
   }

   LM_NOTICE("Incoming call. call_id=%s | src_ip=%s | from_tag=%s | username=%s\n",
		   callID,src_ip,tag,user_name);

   ////////////////////// Check if contains fax attributes ////////////

   if (parse_sdp(msg) < 0)
   {
      LM_ERR("Unable to parse sdp. error_code=%d | message %s\n",PARSER_ERROR,msg_copy);
      return PARSER_ERROR;
   }

   struct sdp_info* sdp;
   sdp = msg->sdp;

   struct sdp_session_cell *sessions;
   sessions = sdp->sessions;

   struct sdp_stream_cell* streams;
   streams = sessions->streams;

   char media_type[32];  //if size value is changed, change also the next if clause
   bzero(media_type,32);

   while(streams!=NULL)
   {
	   if(32 < streams->media.len+1) // if 32 number is changed, change also the size of 'media_type' variable above
	   {
		   LM_ERR("Media type variable ('media_type') is shorter than stream->media, error_code=%d\n",PARSER_ERROR);
		   return PARSER_ERROR;
	   }

	   snprintf(media_type,streams->media.len+1,streams->media.s);
	   LM_INFO("Media type is %s\n",media_type);
	   if(strcmp(media_type,"image")==0)
	   {
		   LM_ERR("Found fax attributes in the message. This feature is not yet supported, call_id=%s | error_code=%d | reason=%s\n",callID,NOT_ACCEPTABLE,"Not Acceptable");
		   return NOT_ACCEPTABLE;
	   }

	   streams=streams->next;
   }


   ////////////////////// Checks if client existes //////////////////

   for (i = 0; i < MAX_CONNECTIONS; i++)
   {
      if (connections[i].s != EMPTY && connections[i].s != TERMINATED && connections[i].s != REFER_TO) // Find an empty connection
      {
         int c = 0;
         for (c = 0; c < MAX_CLIENTS && connections[i].clients[c].is_empty==1; c++) // Checks if all clients are empty
         {
             //Comment to tests, make sure that this validation is required
             /*if( (strcmp(connections[i].call_id,callID)!=0 ) && (strcmp(connections[i].b2b_client_callID,callID)!=0 )){ //Check if is a differente call-id
              LM_ERR("ERROR: Active client with different Call-ID.Cleaning connection %d | state %d. Ip = %s | call-id = %s\n.",
              connections[i].id,connections[i].s,src_ip,callID);
              send_remove_to_xcoder(&(connections[i]));
              cancel_connection(&(connections[i]));
              clean_connection(&(connections[i]));
              return DUPLICATE_CLIENT;
              }
              else*/

            if (strcmp(connections[i].clients[c].src_ip, src_ip) == 0) //check if src ip exists
            	LM_INFO("This ip [%s], is already active in one connection. b2bcallid=%d | call_id=%s\n",src_ip,connections[i].id,callID);

            if ((strcmp(connections[i].cseq, cseq_call) == 0) && ( (strcmp(connections[i].call_id, callID) == 0))) //check if is a repetive invite
            {
            	LM_NOTICE("Repetive invite from %s. dropping message. call_id=%s | from_tag=%s | cseq=%s\n", src_ip,callID,tag,cseq_call);
            	return TO_DROP;
            }
            else if(strcmp(connections[i].b2b_client_callID, callID) == 0)
            {
            	LM_ERR("Detected Invite with callid generated by b2b module.call_id=%s | from_tag=%s | cseq=%s | error_code=%d | reason=%s\n",callID,tag,cseq_call,NOT_ACCEPTABLE,"Not Acceptable");
            	//TODO: Analisar o comportamento do SBC para saber cancelo a chamada ou n�o
//				free_ports_client(&(connections[i].clients[c]));
//				cancel_connection(&(connections[i]));
				return NOT_ACCEPTABLE;
            }
         }
      }
   }

   ////////////////////// Initializes a connection //////////////////
   int session_id = -1;
   status = get_conn_session(&session_id);
   if (status != OK)
   {
      LM_ERR("ERROR. Error finding empty connection slot. error_code=%d.\n", status);
      return INIT_ERROR;
   }

   connection = &(connections[session_id]);
   connection->id = session_id;
   LM_INFO("Seting id to connection. Id = %d\n", i);
   char id[4];
   sprintf(id, "%d%d", session_id, 0);
   clean_client(&(connection->clients[0]));	// Cleaning client structure
   connection->clients[0].id = atoi(id);
   sprintf(connection->cseq, cseq_call);
   sprintf(connection->clients[0].src_ip, src_ip);
   sprintf(connection->clients[0].user_name,user_name);
   sprintf(connection->clients[0].user_agent,user_agent_str);
   sprintf(connection->clients[0].tag, tag);
   connection->clients[0].is_empty = 1;
   connection->clients[0].s = INVITE_C;
   connection->conn_time = time(0);
   sprintf(connection->call_id, callID);

   if (connection == NULL)
   {
      LM_ERR("Error: Error creating connection. call_id=%s | src_ip=%s | from_tag=%s | cseq=%s | user_name=%.*s\n",
    		  callID,src_ip,tag,cseq_call,pfrom->parsed_uri.user.len,pfrom->parsed_uri.user.s);
      return INIT_ERROR;
   }

   LM_NOTICE("Accepting incomming call. b2bcallid=%d | client_id=%d | call_id=%s | username=%s\n",
		   connection->id, connection->clients[0].id,connection->call_id,connection->clients[0].user_name);

   status = parse_sdp_b2b(msg, &message, connection, &(connection->clients[0]));

   /////////////////////////////// Treat different errors /////////////////

   if (status == VIDEO_UNSUPPORTED)
   {
      LM_INFO("Only audio is supported. Proceeding with call\n");
      status = OK;
   }

   if (status != OK)
   {
      LM_ERR("ERROR: Error parsing sip message. error_code=%d\n",status);
      int b2bcallid=connection->id;
      free_ports_client(&(connection->clients[0]));
      clean_connection(connection);
      switch (status)
      {
         case XCODER_CMD_ERROR:
        	LM_ERR("ERROR. Xcoder command error. b2bcallid=%d | error_code=%d | reason=%s\n",b2bcallid,SERVER_INTERNAL_ERROR,"Server Internal Error");
            return SERVER_INTERNAL_ERROR;
         case XCODER_TIMEOUT:
        	LM_ERR("ERROR.Xcoder timeout. b2bcallid=%d | error_code=%d | reason=%s\n",b2bcallid,SERVER_TIME_OUT,"Server Time-out");
            return SERVER_TIME_OUT;
         case NOT_ACCEPTABLE_HERE:
        	LM_ERR("ERROR. Media type is not supported. b2bcallid=%d | error_code=%d | reason=%s\n",b2bcallid,NOT_ACCEPTABLE_HERE,"Not Acceptable here");
            return NOT_ACCEPTABLE_HERE;
         case SERVER_UNAVAILABLE :
        	LM_ERR("ERROR. Service is full. b2bcallid=%d | error_code=%d | reason=%s\n",b2bcallid,SERVER_UNAVAILABLE,"Service Unavailable");
        	return SERVER_UNAVAILABLE;
         case NOT_ACCEPTABLE :
        	LM_ERR("Found fax attributes in the message. This feature is not yet supported, b2bcallid=%d | error_code=%d | reason=%s\n",b2bcallid,NOT_ACCEPTABLE,"Not Acceptable");
        	return NOT_ACCEPTABLE;
         default:
        	LM_ERR("ERROR. Error parsing sip message. b2bcallid=%d | error_code=%d | reason=%s\n",b2bcallid,SERVER_INTERNAL_ERROR,"Server Internal Error");
            return status;
            break;
      }
   }
   check_connections(); //Debug only

   connection->clients[0].s = PENDING_INVITE;
   connection->s = PENDING;

   LM_NOTICE("Successfully accepted incoming call. b2bcallid=%d | client_id=%d\n",connection->id,connection->clients[0].id);
   LM_INFO("Successfully accepted incoming call. conn_state %d | b2bcallid=%d | client_id=%d | client_state=%d | username=%s | src_ip=%s | tag=%s\n"
		   ,connection->s,connection->id, connection->clients[0].id,connection->clients[0].s,connection->clients[0].user_name,connection->clients[0].src_ip,connection->clients[0].tag);
   LM_NOTICE("Sending invite to host=%s | username=%s | b2bcallid=%d\n",dst_host,dst_user_name,connection->id);
   return OK;
}

/******************************************************************************
 *        NAME: general_failure
 * DESCRIPTION: This function is invoked in opensips routine when is detected a message with response code 4xx,5xx or 6xx.
 *              Checks the sate of connection and if necessary, ends the connection.
 *****************************************************************************/

static int
general_failure(struct sip_msg* msg)
{
	LM_INFO("Parse failure\n");
   //Remove \r and \n from sip message

   char msg_copy[msg->len+2];
   int size=sizeof(msg_copy)/sizeof(char);
   if( size >= (msg->len+2) )
   {
	   snprintf(msg_copy,msg->len+1,msg->buf);
	   remove_newline_str(msg_copy);
   }
   else
   {
	   LM_WARN("Failed to allocate space for 'msg_copy' variable\n");
   }


   char * src_ip = ip_addr2a(&msg->rcv.src_ip);
   struct to_body *pfrom; //Structure contFrom header

   if (parse_headers(msg, HDR_CSEQ_F | HDR_TO_F | HDR_FROM_F | HDR_CALLID_F, 0) != 0)
   {
      LM_ERR("ERROR: Error parsing headers. message [%s]\n",msg_copy);
      return INIT_ERROR;
   }

   if (parse_from_header(msg) != 0)
   { // Parse header FROM
      LM_ERR("ERROR: Bad Header. message [%s]\n",msg_copy);
      return INIT_ERROR;
   }
   pfrom = get_from(msg); //Get structure containing parsed From header

   char from_tag[128];
   bzero(from_tag, 128);
   char callID[128];
   bzero(callID, 128);
   snprintf(callID, (msg->callid->body.len + 1), msg->callid->body.s); //Get call-id
   snprintf(from_tag, pfrom->tag_value.len + 1, pfrom->tag_value.s); //Get from tag

   LM_NOTICE("Received '%.*s %.*s' from ip %s | call_id=%s | tag=%s\n",
         msg->first_line.u.reply.status.len, msg->first_line.u.reply.status.s, msg->first_line.u.reply.reason.len, msg->first_line.u.reply.reason.s, src_ip,callID,from_tag);

   int i = 0;
   for (i = 0; i < MAX_CONNECTIONS; i++) //Loop to find right connection
   {
      if (connections[i].s != TERMINATED && connections[i].s != EMPTY
            && ((strcmp(connections[i].b2b_client_callID, callID) == 0) || (strcmp(connections[i].call_id, callID) == 0)))
      { //Compare with ID generated by B2B

         int c = 0;
         for (c = 0; c < MAX_CLIENTS; c++) // Checks if all clients are empty
         {
            LM_INFO("b2bcallid=%d | client_id=%d | tag=%s | b2b_tag=%s | state=%d\n\n",
            		connections[i].id,connections[i].clients[c].id, connections[i].clients[c].tag, connections[i].clients[c].b2b_tag, connections[i].clients[c].s);

            if ((strcmp(connections[i].clients[c].tag, from_tag) == 0) || (strcmp(connections[i].clients[c].b2b_tag, from_tag) == 0)) //This is a b2b generated invite
            {
               if (connections[i].clients[c].s == TO_HOLD || connections[i].clients[c].s == PENDING_HOLD)
               {
                  LM_NOTICE("Failed to put call on hold. b2bcallid=%d\n",connections[i].id);
                  connections[i].clients[c].s = ACTIVE;
                  connections[i].s = ACTIVE;
               }
               else if (connections[i].clients[c].s == ON_HOLD || connections[i].clients[c].s == OFF_HOLD
                     || connections[i].clients[c].s == PENDING_OFF_HOLD)
               {
                  LM_NOTICE("Failed to put call off hold. b2bcallid=%d\n",connections[i].id);
                  connections[i].clients[c].s = ON_HOLD;
                  connections[i].s = ACTIVE;
               }
               else if (connections[i].s == PENDING)
               {
                  LM_NOTICE("Failed to create call.Cleaning connection b2bcallid=%d\n", connections[i].id);
                  free_ports_client(&(connections[i].clients[c]));
                  clean_connection(&(connections[i]));
               }
               else if (connections[i].s == REINVITE)
               {
                  free_ports_client(&(connections[i].clients[c]));
                  LM_NOTICE("Failed to reinvite message.b2bcallid=%d\n", connections[i].id);
                  connections[i].s = ACTIVE;
               }
               else if (connections[i].s == PENDING_EARLY_MEDIA)
               {
                  LM_NOTICE("Failed to create early media. b2bcallid=%d\n", connections[i].id);
                  free_ports_client(&(connections[i].clients[c]));
                  cancel_connection(&(connections[i]));
               }
               i = MAX_CONNECTIONS;
               c = MAX_CLIENTS;
               LM_INFO("b2bcallid=%d | conn_state=%d | client_id=%d | client_state=%d\n",connections[i].id,connections[i].s,connections[i].clients[c].id,connections[i].clients[c].s);
               break;
            }
         }
      }
   }
   check_connections();

   return OK;
}

/******************************************************************************
 *        NAME: parse_refer
 * DESCRIPTION: This function is invoked in opensips routine when a message of type 'refer' is received.
 *              The connection will be used to send/receive information to xcoder.
 *****************************************************************************/

static int
parse_refer(struct sip_msg* msg)
{
   LM_INFO("Parse refer\n");

   int status = OK;
   struct hdr_field* refer;
   char ip[25];
   bzero(ip, 25);
   char callID[128];
   bzero(callID, 128);

   //Remove \r and \n from sip message
   char msg_copy[msg->len+2];
   int size=sizeof(msg_copy)/sizeof(char);
   if( size >= (msg->len+2) )
   {
	   snprintf(msg_copy,msg->len+1,msg->buf);
	   remove_newline_str(msg_copy);
   }
   else
   {
	   LM_WARN("Failed to allocate space for 'msg_copy' variable\n");
   }


   ////////////////// Parse headers //////////////

   if (parse_headers(msg, HDR_REFER_TO_F, 0) != 0)
   {
      LM_ERR("ERROR: Error parsing headers. error_code=%d | message %s\n",INIT_ERROR,msg_copy);
      return INIT_ERROR;
   }

   if (parse_refer_to_header(msg) != 0)
   {
      LM_ERR("ERROR: Error parsing refer_to header. error_code=%d | message %s\n",INIT_ERROR,msg_copy);
      return INIT_ERROR;
   }

   refer = msg->refer_to->parsed;
   snprintf(callID, (msg->callid->body.len + 1), msg->callid->body.s); // Get call-id

   LM_INFO("Refer %.*s\n", refer->body.len, refer->body.s);

   /////////////////// Get ip to transfer call /////////////

   int i = 0;
   while (refer->body.s[i] != '@' && refer->body.s[i] != '\0')
      i++;

   if (refer->body.s[i] == '@')
   {
      i++;
      read_until_char(refer->body.s, &i, ':', ip);
   }
   else
   {
      LM_ERR("Error retrieving destination ip. error_code=%d | message %s\n",INIT_ERROR,msg_copy);
      return INIT_ERROR;
   }

   LM_INFO("IP to transfer call : %s\n", ip);

   //////////////// Find connection and set state to REFER_TO

   for (i = 0; i < MAX_CONNECTIONS; i++)
   {
      if (connections[i].s != EMPTY && connections[i].s != TERMINATED
            && (strcmp(connections[i].call_id, callID) == 0 || strcmp(connections[i].b2b_client_callID, callID) == 0))
      {

    	 LM_INFO("Found connection to transfer. b2bcallid=%d | conn_state=%d | call_id=%s.\n", connections[i].id, connections[i].s,callID);
         LM_NOTICE("Found connection to transfer. b2bcallid=%d | conn_state=%d.\n", connections[i].id, connections[i].s);
         break;
      }
   }

   return status;
}

/******************************************************************************
 *        NAME: create_socket_structuret
 * DESCRIPTION: Initializes fd_socket_list with newly created sockets.
 *              The connections will be used to send/receive information to xcoder.
 *****************************************************************************/

int
create_socket_structures(void)
{
   int i = 0;
   for (i = 0; i < MAX_SOCK_FD; i++)
   {

      //sock_fd = -1;
      struct sockaddr_un peeraddr_un; /* for peer socket address */
      int flags;
      if ((fd_socket_list[i].fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
      {
         LM_ERR("ERROR, Unable to create connection to xcoder. error_code=%d\n",INIT_ERROR);
         return INIT_ERROR;
      }

      memset((char *) &peeraddr_un, 0, sizeof(peeraddr_un));

      peeraddr_un.sun_family = AF_UNIX;
      LM_INFO("Connecting to %s\n", XCODER_UNIX_SOCK_PATH);
      strncpy(peeraddr_un.sun_path, XCODER_UNIX_SOCK_PATH, sizeof(peeraddr_un.sun_path)); // Connect to socket provided by xcoder
      if ((connect(fd_socket_list[i].fd, (struct sockaddr *) &peeraddr_un, sizeof(peeraddr_un))) == -1)
      {
         LM_ERR("ERROR: Unable to connect to socket. error_code=%d\n",INIT_ERROR);
         shutdown(fd_socket_list[i].fd, SHUT_RDWR);
         close(fd_socket_list[i].fd);
         fd_socket_list[i].fd = -1;
         return INIT_ERROR;
      }

      flags = fcntl(fd_socket_list[i].fd, F_GETFL, 0);
      fcntl(fd_socket_list[i].fd, F_SETFL, flags | O_NONBLOCK); // Make non block
      fd_socket_list[i].busy = 0;
   }

   return OK;
}

/******************************************************************************
 *        NAME: parse_bye
 * DESCRIPTION: This function is invoked in opensips routine when a message of
 *              type 'bye' is received.
 *              This function identifies the connections and terminate it.
 *****************************************************************************/

static int
parse_bye(struct sip_msg* msg)
{
   LM_INFO("Parsing bye\n");
   struct to_body *pfrom; //Structure contFrom header

   char msg_copy[msg->len+2];
   int size=sizeof(msg_copy)/sizeof(char);
   if( size >= (msg->len+2) )
   {
	   snprintf(msg_copy,msg->len+1,msg->buf);
	   remove_newline_str(msg_copy);
	   LM_INFO("Received message : [%s]\n", msg_copy);
   }
   else
   {
	   LM_WARN("Failed to allocate space for 'msg_copy' variable\n");
   }

   if (parse_from_header(msg) != 0)
   { // Parse header FROM
      LM_ERR("ERROR: Bad Header. error_code=%d | message %s\n",PARSER_ERROR,msg_copy);
      return PARSER_ERROR;
   }
   pfrom = get_from(msg); //Get structure containing parsed From header

   char tag[128];
   bzero(tag, 128);
   snprintf(tag, pfrom->tag_value.len + 1, pfrom->tag_value.s);
   char * src_ip = ip_addr2a(&msg->rcv.src_ip);
   char callID[128];
   bzero(callID, 128);
   snprintf(callID, (msg->callid->body.len + 1), msg->callid->body.s);

   LM_INFO("Bye message information. src_ip=%s | call_id=%s | from_tag=%s\n", src_ip, callID, tag);

   int i = 0;
   for (i = 0; i < MAX_CONNECTIONS; i++)
   {
      if (connections[i].s != TERMINATED && connections[i].s != EMPTY
            && (strcmp(connections[i].call_id, callID) == 0 || strcmp(connections[i].b2b_client_callID, callID) == 0))
      {
    	 LM_NOTICE("Received bye. b2bcallid=%d | from_tag=%s | src_ip=%s | call_id=%s",connections[i].id,tag,src_ip,callID);
         LM_INFO("Cleaning connection. b2bcallid=%d\n", connections[i].id);
         send_remove_to_xcoder(&(connections[i]));
         clean_connection(&(connections[i])); //Clean connetions that receives a bye
         break;
      }
   }
   if (i == MAX_CONNECTIONS)
   {
      LM_ERR("Error: no connection found for bye. error_code=%d\n",GENERAL_ERROR);
      return GENERAL_ERROR;
   }

   check_connections();
   return OK;

}



/******************************************************************************
 *        NAME: parse_cancel
 * DESCRIPTION: This function is invoked in opensips routine when a message of
 *		type cancel is received.
 *		This function cancel a previous 'invite' request.
 *****************************************************************************/

static int
parse_cancel(struct sip_msg* msg)
{
   LM_INFO("Parsing Cancel\n");
   struct to_body *pfrom; //Structure contFrom header

   //Remove \r and \n from sip message
   char msg_copy[msg->len+2];
   int size=sizeof(msg_copy)/sizeof(char);
   if( size >= (msg->len+2) )
   {
	   snprintf(msg_copy,msg->len+1,msg->buf);
	   remove_newline_str(msg_copy);
	   LM_INFO("Received message : [%s]\n", msg_copy);
   }
   else
   {
	   LM_WARN("Failed to allocate space for 'msg_copy' variable\n");
   }


   if (parse_from_header(msg) != 0)
   { // Parse header FROM
      LM_ERR("ERROR: Bad Header.error_code=%d | message %s\n",PARSER_ERROR,msg_copy);
      return PARSER_ERROR;
   }
   pfrom = get_from(msg); //Get structure containing parsed From header

   char tag[128];
   bzero(tag, 128);
   snprintf(tag, pfrom->tag_value.len + 1, pfrom->tag_value.s);
   char * src_ip = ip_addr2a(&msg->rcv.src_ip);
   char callID[128];
   bzero(callID, 128);
   snprintf(callID, (msg->callid->body.len + 1), msg->callid->body.s);

   LM_INFO("Information CANCEL. call_id=%s | src_ip=%s | tag=%s\n", callID,src_ip, tag);

   int i = 0;
   for (i = 0; i < MAX_CONNECTIONS; i++)
   {
      if (connections[i].s != TERMINATED && connections[i].s != EMPTY
            && (strcmp(connections[i].call_id, callID) == 0 || strcmp(connections[i].b2b_client_callID, callID) == 0))
      {
         LM_NOTICE("Canceling call. b2bcallid=%d | conn_state=%d | src_ip=%s\n", connections[i].id, connections[i].s, src_ip);

         if (connections[i].s == INITIALIZED || connections[i].s == PENDING)
         {
            free_ports_client(&(connections[i].clients[0]));
            clean_connection(&(connections[i]));
            break;
         }
         else if (connections[i].s == PENDING_EARLY_MEDIA)
         {
        	send_remove_to_xcoder(&(connections[i]));
            cancel_connection(&(connections[i]));
            break;
         }
         else if (connections[i].s == EARLY_MEDIA)
         {
        	send_remove_to_xcoder(&(connections[i]));
            cancel_connection(&(connections[i]));
            break;
         }
         else
         {
            send_remove_to_xcoder(&(connections[i]));
            cancel_connection(&(connections[i]));
            LM_ERR("Wrong connection state to receive 'cancel'. b2bcallid=%d | conn_state : %d\n", connections[i].id, connections[i].s);
         }
      }
   }

   return OK;
}

/******************************************************************************
 *        NAME: create_call
 * DESCRIPTION: This functions is responsible to connect two clients.
 * 		Communicate with xcoder and send a command of type 'create', to
 *		create a connection.
 *		
 *		Receives the connection containing the two clients and the caller.
 *****************************************************************************/

int
create_call(conn * connection, client * caller)
{
   LM_INFO("Creating call\n");
   int i = 0;
   int status = OK;

   char payload_A[32];
   char codec_A[64];
   char payload_dtmf_A[32];
   char payload_B[32];
   char codec_B[64];
   char payload_dtmf_B[32];
   bzero(payload_A, 32);
   bzero(codec_A, 64);
   bzero(payload_dtmf_A, 32);
   bzero(payload_B, 32);
   bzero(codec_B, 64);
   bzero(payload_dtmf_B, 32);
   //////////////////////////// Create call command to send xcoder ///////////////////////////

   client * callee = NULL;
   get_client(connection, caller, &callee); //Get destination client
   if (callee == NULL)
   {
      LM_ERR("Error: Error retrieving destination client. b2bcallid=%d | conn_state=%d | call_id=%s | client_id=%d | client state=%d | error_code=%d\n",
    		  connection->id,connection->s,connection->call_id,caller->id,caller->s,GENERAL_ERROR);
      return GENERAL_ERROR;
   }


   xcoder_msg_t msg_x;
   memset(&msg_x, 0, sizeof(xcoder_msg_t));
   msg_x.id = XCODER_MSG_CREATE;
   msg_x.type = XCODER_MSG_TYPE_COMMAND;
   msg_x.params[0].id = XCODER_PARAM_MSG_TYPE;
   msg_x.params[1].id = XCODER_PARAM_MSG_VALUE;
   msg_x.params[2].id = XCODER_PARAM_MSG_COUNT;
   msg_x.params[3].id = XCODER_PARAM_CALLER_ID;
   msg_x.params[4].id = XCODER_PARAM_CALLER_IP;
   msg_x.params[5].id = XCODER_PARAM_CALLER_APORT;
   msg_x.params[6].id = XCODER_PARAM_CALLER_MOTION_APORT;
   msg_x.params[7].id = XCODER_PARAM_CALLER_ACODEC;
   msg_x.params[8].id = XCODER_PARAM_CALLER_APT;
   msg_x.params[9].id = XCODER_PARAM_CALLER_DTMFPT;
   msg_x.params[10].id = XCODER_PARAM_CALLEE_ID;
   msg_x.params[11].id = XCODER_PARAM_CALLEE_IP;
   msg_x.params[12].id = XCODER_PARAM_CALLEE_APORT;
   msg_x.params[13].id = XCODER_PARAM_CALLEE_MOTION_APORT;
   msg_x.params[14].id = XCODER_PARAM_CALLEE_ACODEC;
   msg_x.params[15].id = XCODER_PARAM_CALLEE_APT;
   msg_x.params[16].id = XCODER_PARAM_CALLEE_DTMFPT;
   msg_x.params[17].id = XCODER_PARAM_B2B_CALL_ID;

   int message_number = 0;
   get_and_increment(message_count, &message_number);

   sprintf(msg_x.params[0].text, "command");
   sprintf(msg_x.params[1].text, "create");
   sprintf(msg_x.params[2].text, "%d", message_number);
   sprintf(msg_x.params[3].text, "%d", caller->id);
   sprintf(msg_x.params[4].text, caller->conn_ip);
   sprintf(msg_x.params[5].text, caller->original_port);
   sprintf(msg_x.params[6].text, callee->dst_audio);

   ///////////////////////// GET PAYLOADS (Suported only A to B calls, More than 2 clients is not suported //////////////////

   for (i = 0; i < MAX_PAYLOADS; i++)
   {
      if (caller->payloads[i].is_empty == 1 && (strcmp(caller->payloads[i].codec, "TELEPHONE-EVENT") != 0))
      {
         sprintf(payload_A, caller->payloads[i].payload);
         sprintf(codec_A, caller->payloads[i].codec);
         if(!strcmp(caller->payloads[i].codec, "AMRWB_OA") || !strcmp(caller->payloads[i].codec, "AMRWB_BE"))
         {
            char * ptr_octet;
            ptr_octet = strstr(caller->payloads[i].attr_fmtp_line,"octet-align=1");
            if (ptr_octet != NULL) {
               sprintf(codec_A, "AMRWB_OA");
            }
            else {
               sprintf(codec_A, "AMRWB_BE");
            }
         }


         /// Check if exist other non-empty positions in payloads array
         int k = 0;
         for (k = 0; k < MAX_PAYLOADS; k++) //i+1 to start in the next position
         {
            if (k != i && caller->payloads[k].is_empty == 1 && (strcmp(caller->payloads[k].codec, "TELEPHONE-EVENT") != 0)) //Exclude chosen payload and dtmf
            {
               LM_ERR("ERROR: More than one payload is active from caller. Conn id %d | Client = %s | From_tag : %s.\n",connection->id, caller->src_ip, caller->tag);
               int m = 0;
               for (m = 0; m < MAX_PAYLOADS; m++)
               {
                  LM_NOTICE("Is_empty %d | Codec %s | Payload %s\n",
                        caller->payloads[m].is_empty, caller->payloads[m].codec, caller->payloads[m].payload);
               }

               LM_ERR("caller->payloads[%d].payload : %s.\n", k, caller->payloads[k].payload);
               LM_ERR("caller->payloads[%d].attr_rtpmap_line : %s.\n", k, caller->payloads[k].attr_rtpmap_line);
               LM_ERR("caller->payloads[%d].attr_fmtp_line : %s.\n", k, caller->payloads[k].attr_fmtp_line);
               return GENERAL_ERROR;
            }
         }
         break;
      }
   }

   /////////// Find dtmf payload //////////
   for (i = 0; i < MAX_PAYLOADS; i++)
   {
      if (caller->payloads[i].is_empty == 1 && (strcmp(caller->payloads[i].codec, "TELEPHONE-EVENT") == 0))
      {
         sprintf(payload_dtmf_A, "%s", caller->payloads[i].payload);
         break;
      }
   }

   //If DTM is not present, assign a default value
   if (i == MAX_PAYLOADS)
   {
      LM_INFO("DTMF payload not defined in caller sdp content. Default value is 126\n");
      sprintf(payload_dtmf_A, "%s", "126");
   }

   /////////// Find callee and get payload //////////

   for (i = 0; i < MAX_PAYLOADS; i++)
   {
      if (callee->payloads[i].is_empty == 1 && (strcmp(callee->payloads[i].codec, "TELEPHONE-EVENT") != 0))
      {
         sprintf(payload_B, "%s", callee->payloads[i].payload);
         sprintf(codec_B, "%s", callee->payloads[i].codec);

         /// Check if exist other non-empty positions in payloads array
         int k = 0;
         for (k = 0; k < MAX_PAYLOADS; k++)
         {
            if (k != i && callee->payloads[k].is_empty == 1 && strcmp(callee->payloads[k].codec, "TELEPHONE-EVENT") != 0) //Exclude chosen payload and dtmf codec
            {
               LM_ERR("ERROR: More than one payload is active from callee. Conn id %d | Client = %s | From_tag : %s.\n", connection->id,callee->src_ip, callee->tag);
               int m = 0;
               for (m = 0; m < MAX_PAYLOADS; m++)
               {
                  LM_NOTICE("Is_empty %d | Codec %s | Payload %s\n",
                        callee->payloads[m].is_empty, callee->payloads[m].codec, callee->payloads[m].payload);
               }
               LM_ERR(" caller->payloads[%d].payload : %s.\n", k, callee->payloads[k].payload);
               LM_ERR(" caller->payloads[%d].payload : %s.\n", k, callee->payloads[k].attr_rtpmap_line);
               LM_ERR(" caller->payloads[%d].payload : %s.\n", k, callee->payloads[k].attr_fmtp_line);
               return GENERAL_ERROR;
            }
         }
         break;
      }
   }

   /////////// Find dtmf payload //////////
   for (i = 0; i < MAX_PAYLOADS; i++)
   {
      if (callee->payloads[i].is_empty == 1 && (strcmp(callee->payloads[i].codec, "TELEPHONE-EVENT") == 0))
      {
         sprintf(payload_dtmf_B, "%s", callee->payloads[i].payload);
         break;
      }
   }

   //If DTM is not present, assign a default value
   if (i == MAX_PAYLOADS)
   {
      LM_INFO("DTMF payload not defined in callee sdp content. Default value is 126\n");
      sprintf(payload_dtmf_B, "%s", "126");
   }

   sprintf(msg_x.params[7].text, codec_A);
   sprintf(msg_x.params[8].text, payload_A);
   sprintf(msg_x.params[9].text, payload_dtmf_A);
   sprintf(msg_x.params[10].text, "%d", callee->id);
   sprintf(msg_x.params[11].text, callee->conn_ip);
   sprintf(msg_x.params[12].text, callee->original_port);
   sprintf(msg_x.params[13].text, caller->dst_audio);
   sprintf(msg_x.params[14].text, codec_B);
   sprintf(msg_x.params[15].text, payload_B);
   sprintf(msg_x.params[16].text, payload_dtmf_B);
   sprintf(msg_x.params[17].text, "%d", connection->id);

   LM_NOTICE("Creating call: b2bcallid=%d | caller_id=%d | codec=%s | payload=%s | DTMF_payload=%s | callee_id=%d | codec=%s | payload=%s | DTMF_payload=%s",
		   connection->id,caller->id,codec_A,payload_A,payload_dtmf_A,callee->id,codec_B,payload_B,payload_dtmf_B);

   //////////////////////////// Send message command to xcoder ///////////////////////////////

   char buf[XCODER_MAX_MSG_SIZE];
   bzero(buf, XCODER_MAX_MSG_SIZE);

   char buffer_recv[XCODER_MAX_MSG_SIZE];
   bzero(buffer_recv, XCODER_MAX_MSG_SIZE);

   xcoder_encode_msg((char *) buf, msg_x.id, (xcoder_param_t *) msg_x.params, msg_x.type);
   status = talk_to_xcoder(buf, message_number,buffer_recv );

   if (status != OK)
   {
      LM_ERR("ERROR. Error interacting with xcoder. b2bcallid=%d | conn_state=%d | call_id=%s | caller_state=%d | callee_state=%d | error_code=%d\n",
    		  connection->id,connection->s,connection->call_id,caller->s,callee->s,status);
      return status;
   }

   status = get_response_status(buffer_recv, connection);
   if (status != OK)
   {
      LM_ERR("ERROR. Bad responde by xcoder. b2bcallid=%d | conn_state=%d | call_id=%s | caller_state=%d | callee_state=%d | error_code=%d\n",
    		  connection->id,connection->s,connection->call_id,caller->s,callee->s,status);
      return status;
   }

   return status;
}

/******************************************************************************
 *        NAME: parse_ACK
 * DESCRIPTION: This function is invoked in opensips routine when a message 
 *		'ack' arrives. Verifies the state of the connection and if needed
 *		establish the connection between two clients.              
 *****************************************************************************/

static int
parse_ACK(struct sip_msg* msg)
{
   LM_INFO("Parsing ACK\n");
   struct to_body *pfrom; //Structure contFrom header
   struct cseq_body * cseq;

   //Remove \r and \n from sip message

   char msg_copy[msg->len+2];
   int size=sizeof(msg_copy)/sizeof(char);
   if( size >= (msg->len+2) )
   {
	   snprintf(msg_copy,msg->len+1,msg->buf);
	   remove_newline_str(msg_copy);
	   LM_INFO("Received message : [%s]\n", msg_copy);
   }
   else
   {
	   LM_WARN("Failed to allocate space for 'msg_copy' variable\n");
   }

   if (parse_headers(msg, HDR_CSEQ_F | HDR_TO_F | HDR_FROM_F, 0) != 0)
   {
      LM_ERR("ERROR: Error parsing headers. error_code=%d\n",PARSE_ERROR);
      return PARSE_ERROR;
   }

   if (parse_from_header(msg) != 0)
   { // Parse header FROM
      LM_ERR("ERROR: Bad Header. error_code=%d\n",PARSER_ERROR);
      return PARSER_ERROR;
   }
   pfrom = get_from(msg); //Get structure containing parsed From header
   cseq = get_cseq(msg);

   char from_tag[128];
   char cseq_call[128];
   char * src_ip = ip_addr2a(&msg->rcv.src_ip);
   char callID[128];
   bzero(from_tag, 128);
   bzero(cseq_call, 64);
   bzero(callID, 128);

   snprintf(from_tag, pfrom->tag_value.len + 1, pfrom->tag_value.s);
   snprintf(callID, (msg->callid->body.len + 1), msg->callid->body.s);
   sprintf(cseq_call, "%.*s %.*s", cseq->number.len, cseq->number.s, cseq->method.len, cseq->method.s);

   conn * connection = NULL;
   client * caller = NULL;
   client * cli_dst = NULL;
   int status = OK; // Holds the status of operations

   char copy[64];
   bzero(copy, 64);
   sprintf(copy, cseq_call);
   char * method; // Needed for strtok_r
   char * number;
   number = strtok_r(copy, " ", &method); // Divide a string into tokens.

   /*        str b2b_key;

    int p=0;
    for(p=0;p<MAX_CONNECTIONS;p++)  // Loop through the connections array
    {
    if( (strcmp(connections[p].call_id,callID)==0)){
    b2b_key.s=connections[p].b2b_client_callID;
    b2b_key.len=strlen(connections[p].b2b_client_callID);
    }
    }

    char * tmp = "str";
    str st;
    st.s=tmp;
    st.len=strlen(tmp);

    b2b_rpl_data_t rpl_data;
    memset(&rpl_data, 0, sizeof(b2b_rpl_data_t));
    rpl_data.et=B2B_CLIENT;
    rpl_data.b2b_key=&b2b_key;
    rpl_data.code =2;
    rpl_data.text =&st;
    b2b_api.send_reply(&rpl_data);*/

   LM_INFO("Information from ACK. call_id=%s | src_ip=%s | from_tag=%s\n",callID, src_ip, from_tag);

   /////////////////////////////////////  Find a client and validate connection state ///////////////////////////

   int i = 0;
   for (i = 0; i < MAX_CONNECTIONS; i++)
   {
      if ((connections[i].s == CREATING || connections[i].s == EARLY_MEDIA || connections[i].s == REINVITE)
            && (strcmp(connections[i].call_id, callID) == 0 || strcmp(connections[i].b2b_client_callID, callID) == 0))
      {
         connection = &(connections[i]);
         LM_INFO("Found connection. b2bcallid=%d | call_id=%s | b2b_gen_call_id=%s | conn_state=%d\n",
        		 connections[i].id,connections[i].call_id, connections[i].b2b_client_callID, connections[i].s);

         int c = 0;
         for (c = 0; c < MAX_CLIENTS; c++) //Find an empty client whitin a pending connection
         {
            if (connection->clients[c].is_empty == 1 && (strcmp(connection->clients[c].tag, from_tag) == 0))
            {
               caller = &(connections[i].clients[c]);
               LM_NOTICE("Received ACK from known client. b2bcallid=%d | client_id=%d | tag=%s | username=%s\n",
                           		   connection->id,caller->id,caller->tag,caller->user_name);
               LM_INFO("Received ACK from known client. b2bcallid=%d | conn_state=%d | client_id=%d | state=%d | tag=%s | username=%s\n",
            		   connection->id,connection->s,caller->id,caller->s,caller->tag,caller->user_name);
               get_client(connection, caller, &cli_dst); // Get destination client to insert the chosen payload
               if (cli_dst == NULL)
               {
                  ////// Terminate connection
                  LM_ERR("ERROR: No destination client found. b2bcallid=%d | conn_state=%d | call_id=%s | b2b_gen_call_id=%s | client_id=%d | state=%d | src_ip %s | username=%s | error_code=%d\n",
                		  connection->id,connection->s,connection->call_id,connection->b2b_client_callID,caller->id,caller->s,caller->src_ip, caller->user_name,PARSER_ERROR);
                  send_remove_to_xcoder(connection);
                  cancel_connection(connection);
                  return PARSER_ERROR;
               }

               ///////// Get cseq and check flow of messages

               char second_copy[64];
               bzero(second_copy, 64);
               sprintf(second_copy, connection->cseq);
               char * method_previous; // Needed for strtok_r
               char * number_previous;
               number_previous = strtok_r(second_copy, " ", &method_previous); // Divide a string into tokens.

               if ((strcmp(method_previous, "INVITE") != 0)
                     && (caller->s != PENDING_200OK || caller->s == PENDING_HOLD || caller->s == PENDING_OFF_HOLD))
               {
                  LM_NOTICE("Uninteresting message. b2bcallid=%d | conn_state=%d | caller_id=%d | caller_state=%d | callee_id=%d | callee_state=%d\n",
                		  connection->id,connection->s,caller->id,caller->s,cli_dst->id,cli_dst->s);
                  return OK;
               }
               else if (caller->s == PENDING_HOLD)
               {
                  LM_NOTICE("Call on Hold. b2bcallid=%d | client_id=%d | username=%s\n",connection->id,caller->id,caller->user_name);
                  LM_INFO("Call on Hold. b2bcallid=%d | conn_state=%d | call_id=%s | b2b_gen_call_id=%s | client_id=%d | state=%d | tag=%s | username=%s. caller port : %s | callee port %s\n",
                		  connection->id,connection->s,connection->call_id,connection->b2b_client_callID,caller->id,caller->s,caller->tag,caller->user_name,caller->dst_audio, cli_dst->dst_audio);
                  sprintf(connection->cseq, cseq_call); // Update cseq header
                  send_remove_to_xcoder(connection); // Send remove to xcoder
                  i = MAX_CONNECTIONS; // Leave the cycle
                  c = MAX_CLIENTS; // Leave the cycle
               }
               else if (caller->s == PENDING_OFF_HOLD)
               {
                  LM_NOTICE("Call is off Hold. b2bcallid=%d | client_id=%d | username=%s\n",connection->id,caller->id,caller->user_name);
                  LM_INFO("Call on Hold. b2bcallid=%d | conn_state=%d | call_id=%s | b2b_gen_call_id=%s | client_id=%d | state=%d | tag=%s | username=%s. Caller port : %s | Calee port %s\n",
                		  connection->id,connection->s,connection->call_id,connection->b2b_client_callID,caller->id,caller->s,caller->tag,caller->user_name,caller->dst_audio, cli_dst->dst_audio);
                  sprintf(connection->cseq, cseq_call); // Update cseq header
                  send_remove_to_xcoder(connection); // Send remove to xcoder

                  i = MAX_CONNECTIONS; // To leave the cycle
                  c = MAX_CLIENTS;
               }
               else if (connections[i].s == REINVITE)
               {
                  LM_NOTICE("Creating call after a reinvite. b2bcallid=%d | client_id=%d | username=%s\n",connection->id,caller->id,caller->user_name);
                  LM_INFO("Creating call after a reinvite. b2bcallid=%d | conn_state=%d | call_id=%s | b2b_gen_call_id=%s | client_id=%d | state=%d | tag=%s | username=%s. caller port : %s | callee port %s\n",
                                  		  connection->id,connection->s,connection->call_id,connection->b2b_client_callID,caller->id,caller->s,caller->tag,caller->user_name,caller->dst_audio, cli_dst->dst_audio);
                  sprintf(connection->cseq, cseq_call); // Update cseq header
                  send_remove_to_xcoder(connection); // Send remove to xcoder

                  i = MAX_CONNECTIONS; // To leave the cicle
                  c = MAX_CLIENTS;
               }
               else if (connections[i].s == EARLY_MEDIA)
               {
                  LM_NOTICE("Creating call after early media. b2bcallid=%d | client_id=%d | username=%s\n",connection->id,caller->id,caller->user_name);
                  LM_INFO("Creating call after early media. b2bcallid=%d | conn_state=%d | call_id=%s | b2b_gen_call_id=%s | client_id=%d | state=%d | tag=%s | username=%s. caller port : %s | callee port %s\n",
                		  connection->id,connection->s,connection->call_id,connection->b2b_client_callID,caller->id,caller->s,caller->tag,caller->user_name,caller->dst_audio, cli_dst->dst_audio);
                  sprintf(connection->cseq, cseq_call); // Update cseq header
                  send_remove_to_xcoder(connection); // Send remove to xcoder

                  i = MAX_CONNECTIONS; // To leave the cicle
                  c = MAX_CLIENTS;
               }
               else if (connections[i].s == CREATING)
               {
                  LM_INFO("Creating call. b2bcallid=%d | caller_id=%d | callee_id=%d\n", connection->id,caller->id,cli_dst->id);
                  sprintf(connections[i].cseq, cseq_call);

                  i = MAX_CONNECTIONS; // To leave the cycle
                  c = MAX_CLIENTS;
               }
               else
               {
            	  LM_INFO("Nothing to do. b2bcallid=%d | conn_state=%d | call_id=%s | b2b_gen_call_id=%s | client_id=%d | state=%d | tag=%s | username=%s. caller port : %s | callee port %s\n",
            			  connection->id,connection->s,connection->call_id,connection->b2b_client_callID,caller->id,caller->s,caller->tag,caller->user_name,caller->dst_audio, cli_dst->dst_audio);
                  return OK;
               }
            }
         }
      }
   }

   if (connection == NULL)
   {
      LM_NOTICE("No connection found. call_id=%s | src_ip=%s\n",callID, src_ip);
      return GENERAL_ERROR;
   }

   if(caller == NULL)
   {
	   LM_ERR("No client found on connection. b2bcallid=%d | call_id=%s | b2b_gen_call_id=%s | src_ip=%s\n",
			   connection->id, connection->call_id, connection->b2b_client_callID, src_ip);
	   int m=0;
	   for(m=0;m<MAX_CLIENTS;m++)
	   {
		   if(connection->clients[m].is_empty == 1)
		   {
			   LM_ERR("Client info for b2bcallid=%d | client id=%d | state=%d | src_ip=%s | tag=%s | b2b_tag=%s | error_code=%d\n",
					   connection->id,connection->clients[m].id,connection->clients[m].s,connection->clients[m].src_ip,connection->clients[m].tag,connection->clients[m].b2b_tag,GENERAL_ERROR);
			   free_ports_client(&(connection->clients[m]));
		   }
		   cancel_connection(connection);
	   }

	   return GENERAL_ERROR;
   }

   LM_INFO("Creating call for connection. b2bcallid=%d\n", connection->id);

   status = create_call(connection, caller); // Send create command to xcoder

   // Check for errors in creating call
   if (status != OK)
   {
      LM_NOTICE("Error creating call.b2bcallid=%d | conn_state=%d | call_id=%s | caller state=%d | callee_state %d\n",
            connection->id, connection->s, connection->call_id,caller->s, cli_dst->s);

      free_ports_client(caller);
      free_ports_client(cli_dst);

      switch (caller->s)
      {
         case PENDING_HOLD:
            LM_NOTICE("Failed to put call on hold.Caller %d remains Active\n", caller->id);
            caller->s = ACTIVE_C;
            return status;
            break;
         case PENDING_OFF_HOLD:
            LM_NOTICE("Failed to put call off hold.Caller %d remains on hold\n", caller->id);
            caller->s = ON_HOLD;
            return status;
            break;
         default:
         {
            switch (connection->s)
            {
               case REINVITE:
                  LM_NOTICE("Failed to process reinvite.Caller %d is Active.\n", caller->id);
                  caller->s = ACTIVE_C;
                  return status;
                  break;
               case CREATING:
                  LM_NOTICE("Failed to create call\n");
                  break;
               default:
                  LM_ERR("Wrong connection state to create call. Conn id %d | conn_state=%d | Conn call_id=%s | Src client_id=%d | Src client state=%d | Src client state ip %s |error_code=%d\n",
                		  connection->id,connection->s,connection->call_id,caller->id,caller->s,caller->src_ip,status);
                  cancel_connection(connection);
                  return PARSER_ERROR;
            }
            break;
         }
      }

      switch (status)
      {
         case XCODER_CMD_ERROR:
            cancel_connection(connection);
            return XCODER_CMD_ERROR;
            break;
         case XCODER_TIMEOUT:
            cancel_connection(connection);
            return XCODER_TIMEOUT;
            break;
         default:
            cancel_connection(connection);
            return SERVER_INTERNAL_ERROR;
            break;
      }
   }

   // Update clients state
   switch (caller->s)
   {
      case PENDING_HOLD:
         caller->s = ON_HOLD;
         LM_NOTICE("Call is on hold b2bcallid=%d.Caller_id=%d is on hold\n", connection->id,caller->id);
         break;
      case PENDING_OFF_HOLD:
         caller->s = ACTIVE_C;
         LM_NOTICE("Caller_id=%d is off hold. b2bcallid=%d\n", caller->id,connection->id);
         break;
      default:
      {
         switch (connection->s)
         {
            case REINVITE:
               LM_NOTICE("Call from reinvite is established. b2bcallid=%d | caller_id=%d | callee_id=%d\n", connection->id,caller->id, cli_dst->id);
               caller->s = ACTIVE_C;
               break;
            case EARLY_MEDIA:
               ; // Behaves as CREATING
            case CREATING:
               LM_NOTICE("Call is established. b2bcallid=%d | caller_id=%d | callee_id=%d\n", connection->id,caller->id, cli_dst->id);
               caller->s = ACTIVE_C;
               cli_dst->s = ACTIVE_C;
               break;
            default:
               LM_ERR("Wrong connection state to create call. b2bcallid=%d | conn_state=%d | call_id=%s | caller_id=%d | caller state=%d | caller_ip %s | callee_id=%d | callee state=%d | callee_ip %s | error_code=%d\n",
            		   connection->id,connection->s,connection->call_id,caller->id,caller->s,caller->src_ip,cli_dst->id,cli_dst->s,cli_dst->src_ip,PARSER_ERROR);
               return PARSER_ERROR;
         }
         break;
      }
   }
   connection->s = ACTIVE;
   LM_INFO("Call info. b2bcallid=%d | conn_state=%d | caller_id=%d | caller_state=%d | callee_id=%d | callee_state=%d\n",
       		  connection->id,connection->s,caller->id,caller->s,cli_dst->id,cli_dst->s);

   return OK;
}

/******************************************************************************
 *        NAME: get_wms_conf
 * DESCRIPTION: This function opens wms configuration file and retrieves the <config_name> value
 * 		
 *****************************************************************************/

int
get_wms_conf(char * filename, char * config_name, char * config_value)
{
   int status = OK, i = 0;
   int LINE_LENGTH = 300;
   char line[LINE_LENGTH], temp[LINE_LENGTH];
   FILE *fp;

   if (!(fp = fopen(filename, "r")))
   {
      LM_ERR("Error opening. Filename %s\n",config_name);
      return INIT_ERROR;
   }

   memset(line, 0, sizeof(line));

   while (fgets(line, LINE_LENGTH, fp) != NULL)
   {
      i = 0;
      memset(temp, 0, sizeof(temp));

      if (line[0] == '#') // Optimize search, if beginning is #, advance to next line
      {
         memset(line, 0, sizeof(line));
         continue;
      }

      if (get_word(line, &i, temp) != OK)
      { // get first word of line
         LM_ERR("Error retrieving wms configuration. Filename %s\n",config_name);
         memset(line, 0, sizeof(line));
         break;
      }

      if (strcmp(temp, config_name) != 0) // Match if is the variable we are looking for
      {
         memset(line, 0, sizeof(line));
         continue;
      }

      status = read_until_char(line, &i, '=', temp);
      if (status != OK)
         break;

      i++; //Advance one position to advance '='

      ////// Remove spaces //////////
      while (line[i] == ' ' && line[i] != '\n' && line[i] != '\r' && line[i] != '\0')
         i++;

      if (line[i] == '\n' || line[i] == '\r' || line[i] == '\0')
      {
         LM_ERR("ERROR. Reached end of line without finding configuration. Filename %s | Config name %s .\n",filename,config_name);
         status = PARSER_ERROR;
         break;
      }

      /////	Get ip ////

      bzero(temp, LINE_LENGTH);
      if (get_word(line, &i, temp) == OK)
      {
         LM_INFO("Configuration retrieved with success. %s: %s\n", config_name, temp);
         sprintf(config_value,"%s", temp);
         break;
      }
      else
      {
         LM_ERR("Error retrieving wms configuration %s\n",config_name);
         status = PARSER_ERROR;
         break;
      }

      memset(line, 0, sizeof(line));
   }

   fclose(fp);
   return status;
}

static int check_overtime_conns(void)
{
	LM_INFO("Checking for overtimed connections\n");
	time_t current_time;
	current_time = time(0);
	int i = 0;
	for (i = 0; i < MAX_CONNECTIONS; i++)
	{
		if (connections[i].s != TERMINATED && connections[i].s != EMPTY)
		{
			if(current_time!=0 && connections[i].conn_time != 0 && current_time-connections[i].conn_time > (*g_connection_timeout) )
			{
				LM_NOTICE("Found overtimed connection: b2bcallid=%d | uptime=%ld\n",connections[i].id,current_time-connections[i].conn_time);

				switch(connections[i].s)
				{
					case INITIALIZED :
					case PENDING :
					case CREATING :
					case PENDING_EARLY_MEDIA :
					{
						int k=0;
						for(k=0;k<MAX_CLIENTS;k++)
						{
							if(connections[i].clients[k].is_empty == 1)
							{
								LM_NOTICE("Cleaning connection b2bcallid=%d | client_id%d\n",connections[i].id,connections[i].clients[k].id);
								free_ports_client(&(connections[i].clients[k]));
							}
						}
					}
					cancel_connection(&(connections[i]));
					break;

					case EARLY_MEDIA :
					case ACTIVE :
					case REINVITE :
					default :
					{
						send_remove_to_xcoder(&(connections[i]));
						cancel_connection(&(connections[i]));
					}
					break;
				}
				continue;
			}
			LM_INFO("Connection start_time %ld | connection uptime %ld\n",connections[i].conn_time,current_time-connections[i].conn_time);
		}
	}
	return OK;
}

/******************************************************************************
 *        NAME: mi_xcoder_b2b_update_codecs
 * DESCRIPTION: This function is called at runtime using mi_fifo module.
 *
 *****************************************************************************/

static struct mi_root* mi_xcoder_b2b_update_codecs(struct mi_root* cmd, void* param)
{
	struct mi_root *rpl_tree;

	rpl_tree = init_mi_tree( 200, MI_OK_S, MI_OK_LEN);
	if (rpl_tree==NULL) return NULL;

	int status=OK;

	char buffer_sent[XCODER_MAX_MSG_SIZE];
	char buffer_recv[XCODER_MAX_MSG_SIZE];
	bzero(buffer_recv, XCODER_MAX_MSG_SIZE);
	bzero(buffer_sent, XCODER_MAX_MSG_SIZE);
	memset(codecs,0,sizeof(codecs));

	int i=0;
	for(i = 0; i < MAX_PAYLOADS; i++)
	{
		if (codecs[i].is_empty == 1)
		{
			codecs[i].is_empty = 0;
		}
	}

   int message_number = 0;
   get_and_increment(message_count, &message_number); // Store message count in message number and increment message_count, counts the number of communications with xcoder

   sprintf(buffer_sent, "xcoder/1.0\r\nmsg_type=command\r\nmsg_value=get_codecs\r\nmsg_count=%d\r\n<EOM>\r\n", message_number); // Command to send to xcoder

   status = talk_to_xcoder(buffer_sent, message_number,buffer_recv);

   if (status != OK)
   {
	  LM_NOTICE("Error interacting with xcoder.\n");
	  return NULL;
   }

   status = parse_codecs(buffer_recv);

   if (status != OK)
   {
	  LM_NOTICE("Error parsing response from xcoder.error_code=%d\n", status);
	  return NULL;
   }

   char codec[256];
   char tmp_codec[256];
   bzero(codec, 256);

   int codec_number = 0;
   for (i = 0; i < MAX_PAYLOADS; i++)
   {
	  if (codecs[i].is_empty == 1)
	  {
		  bzero(tmp_codec, 256);
		  snprintf(tmp_codec,strlen(codec)+1,codec);
		  snprintf(codec,256,"%s %s",tmp_codec,codecs[i].sdpname);
		  codec_number++;
	  }
   }
   LM_NOTICE("Supported codecs are %s:\n",codec);

   if (codec_number < 1)
   {
	  LM_ERR("ERROR. No codecs readed\n");
	  return NULL;
   }

	return rpl_tree;
}


/******************************************************************************
 *        NAME: mod_init
 * DESCRIPTION: This function is invoked when the module is launched.
 *		It is responsible to initialize structures, create shared memory space,
 *		initialize locks and load functions from other modules.
 *****************************************************************************/

static int
mod_init(void)
{
   LM_NOTICE("Initializing xcoder_b2b module.\n");
   LM_NOTICE("Configuration file is : %s\n", conf_file);

   int status = OK; // Holds the status of operations

   ///////////////// Creating locks  /////////////////////

   conn_lock = lock_alloc(); // Initializes a lock to handle message_count
   if (lock_init(conn_lock) == 0)
   {
      LM_ERR("could not initialize a lock\n");
      lock_dealloc(conn_lock);
      return INIT_ERROR;
   }

   ///////////////// Creating shared space for connections information /////////////////////

//        conn tmp_connections[MAX_CONNECTIONS];
//        connections = shm_malloc(sizeof(tmp_connections));
//        memcpy(connections,&tmp_connections,sizeof(tmp_connections));

   connections = shm_malloc(MAX_CONNECTIONS * sizeof(conn));

   codecs = shm_malloc(MAX_PAYLOADS * sizeof(media_relay_codecs));
   memset(codecs,0,sizeof(codecs));

   int i = 0;
   for (i = 0; i < MAX_CONNECTIONS; i++)
   {
      int k = 0;
      for (k = 0; k < MAX_CLIENTS; k++)
      {
         client * cli = shm_malloc(sizeof(client));
         memcpy(&(connections[i].clients[0]), cli, sizeof(client));
      }
   }

   //////// Create socket structures

   socket_list tmp_fd_list[MAX_SOCK_FD];
   fd_socket_list = shm_malloc(sizeof(tmp_fd_list));
   memcpy(fd_socket_list, &tmp_fd_list, sizeof(tmp_fd_list));

   status = create_socket_structures();
   if (status != OK)
   {
      LM_ERR("Failed to create socket structure. Errocode : %d\n", status);
      return INIT_ERROR;
   }

   ///////// Create socket lock

   socket_lock = lock_alloc(); // Initializes a lock to handle socket retrieval in fd_socket_list array
   if (lock_init(socket_lock) == 0)
   {
      LM_ERR("could not initialize socket_lock\n");
      lock_dealloc(socket_lock);
      return INIT_ERROR;
   }

   init_lock = lock_alloc();
   if (lock_init(init_lock) == 0)
   {
      LM_ERR("could not initialize a init_lock\n");
      lock_dealloc(init_lock);
      return INIT_ERROR;
   }

   message_count = shm_malloc(sizeof(int));
   *message_count = 1;

   conn_last_empty = shm_malloc(sizeof(int));
   *conn_last_empty = 0;

   socket_last_empty = shm_malloc(sizeof(int));
   *socket_last_empty = 0;

   media_relay = shm_malloc(32 * sizeof(char)); //Create space for media server ip
   get_wms_conf(conf_file, "sm.server.ip", media_relay); //Read file and retrieve media server ip
   g_connection_timeout = shm_malloc(32 * sizeof(int)); //Create space for media server ip
   char temp[64];
   get_wms_conf(conf_file, "sm.session.timeout", temp); //Read file and retrieve media server ip
   *g_connection_timeout = atoi(temp);

   LM_NOTICE("********** CONFIGURATIONS **********\n");
   LM_NOTICE("media_relay_ip: %s | connection_timeout: %d\n", media_relay, (*g_connection_timeout) );
   LM_NOTICE("************************************\n");


   if (connections == NULL)
   {
      LM_ERR("Failed do allocate structures space\n");
      return INIT_ERROR;
   }

   if (init_structs() != OK)
   {
      LM_ERR("Error initializing structures\n");
      return INIT_ERROR;
   }

   // Load function from other modules (b2b_logic, b2b_entities)

   if (load_b2b_logic_api(&b2b_logic_load) < 0)
   {
      LM_ERR("can't load b2b_logic functions\n");
      return -1;
   }

   if (load_b2b_api(&b2b_api) < 0)
   {
      LM_ERR("Failed to load b2b api\n");
      return -1;
   }

   char buffer_sent[XCODER_MAX_MSG_SIZE];
   bzero(buffer_sent, XCODER_MAX_MSG_SIZE);
   char buffer_recv[XCODER_MAX_MSG_SIZE];
   bzero(buffer_recv, XCODER_MAX_MSG_SIZE);

   int message_number = 0;
   get_and_increment(message_count, &message_number); // Store message count in message number and increment message_count, counts the number of communications with xcoder

   sprintf(buffer_sent, "xcoder/1.0\r\nmsg_type=command\r\nmsg_value=get_codecs\r\nmsg_count=%d\r\n<EOM>\r\n", message_number); // Command to send to xcoder

   status = talk_to_xcoder(buffer_sent, message_number,buffer_recv);

   if (status != OK)
   {
      LM_NOTICE("Error interacting with xcoder.\n");
      return status;
   }

   status = parse_codecs(buffer_recv);

   if (status != OK)
   {
      LM_NOTICE("Error parsing response from xcoder.error_code=%d\n", status);
      return status;
   }

   char codec[256];
   char tmp_codec[256];
   bzero(codec, 256);

   int codec_number = 0;
   for (i = 0; i < MAX_PAYLOADS; i++)
   {
	  if (codecs[i].is_empty == 1)
	  {
		  bzero(tmp_codec, 256);
		  snprintf(tmp_codec,strlen(codec)+1,codec);
		  snprintf(codec,256,"%s %s",tmp_codec,codecs[i].sdpname);
		  codec_number++;
	  }
   }
   LM_NOTICE("Supported codecs are %s:\n",codec);

   if (codec_number < 1)
   {
	  LM_ERR("ERROR. No codecs readed\n");
	  return -1;
   }

   LM_NOTICE("Module xcoder_b2b loaded successfully\n");
   return 0;
}

/******************************************************************************
 *        NAME: mod_destroy
 * DESCRIPTION: Cleans structures and frees the memory previously allocated
 *****************************************************************************/

static void
mod_destroy(void)
{
   LM_NOTICE("CLEANING XCODER_B2B\n");
   if(connections!=NULL)
   {
		int i=0;
		for (i = 0; i < MAX_CONNECTIONS; i++)
		{
			int k = 0;
			for (k = 0; k < MAX_CLIENTS; k++)
			{
				memset(&(connections[i].clients[k]), 0, sizeof(&(connections[i].clients[k])));
				shm_free(&(connections[i].clients[k]));
			}
		}
		memset(connections, 0, sizeof(connections));
		shm_free(connections);
   }
   if(fd_socket_list!=NULL)
   {
	   int i = 0;
	   for (i = 0; i < MAX_SOCK_FD; i++)
	   {
	      shutdown(fd_socket_list[i].fd, SHUT_RDWR);
	      close(fd_socket_list[i].fd);
	   }
	   memset(fd_socket_list, 0, sizeof(fd_socket_list));
	   shm_free(fd_socket_list);
   }
   if(message_count!=NULL)
   {
	   memset(message_count, 0, sizeof(message_count));
	   shm_free(message_count);
   }
   if(conn_last_empty!=NULL)
   {
	   memset(message_count, 0, sizeof(conn_last_empty));
	   shm_free(conn_last_empty);
   }
   if(socket_last_empty!=NULL)
   {
	   memset(message_count, 0, sizeof(socket_last_empty));
	   shm_free(socket_last_empty);
   }
   if(media_relay!=NULL)
   {
	   memset(message_count, 0, sizeof(media_relay));
	   shm_free(media_relay);
   }
   if(g_connection_timeout!=NULL)
   {
	   memset(message_count, 0, sizeof(g_connection_timeout));
	   shm_free(g_connection_timeout);
   }
   if(codecs!=NULL)
   {
	   memset(codecs, 0, sizeof(codecs));
	   shm_free(codecs);
   }
}
