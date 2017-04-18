
#include <stdio.h>
#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <time.h>
#include <curses.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <iostream>
#include <vector>
#include <algorithm>
#include <string>

#define MAX_RESPONSE (20480)

using namespace std;

GtkListStore * list_rooms;
GtkWidget *window;
GtkWidget *tree_view;
GtkWidget *messages_1;
GtkListStore * list_users;
GtkWidget *table;
GtkTreeSelection *treeSel;
GtkWidget *view;
GtkWidget *viewUser;
GtkWidget *userName; //entry
GtkWidget *passWord; //entry
GtkWidget *currentStatus; //label
GtkWidget *entryRoom; //entry
GtkWidget *messageEntry;
vector<string> roomVec;
vector<string> roomVecNew;
GtkWidget *roomUser;

char * host = strdup("localhost");
char * user;
char * password;
char * args;
int port = 3048;
bool loggedIn = false;
bool roomChange = false;

int open_client(char * host, int port) {
	struct sockaddr_in socketAddress;
	memset((char *)&socketAddress, 0, sizeof(socketAddress));
	socketAddress.sin_family = AF_INET;
	socketAddress.sin_port = htons((u_short)port);
	struct hostent *ptrh = gethostbyname(host);
	if (ptrh == NULL) {
	    perror("gethostbyname");
	    exit(1);
	}
	memcpy(&socketAddress.sin_addr, ptrh->h_addr, ptrh->h_length);
	struct  protoent *ptrp = getprotobyname("tcp");
  	if (ptrp == NULL) {
      		perror("getprotobyname");
          	exit(1);
	}
	int sock = socket(PF_INET, SOCK_STREAM, ptrp->p_proto);
	if (sock < 0) {
		perror("socket");
	        exit(1);
        }
	if (connect(sock, (struct sockaddr *)&socketAddress,
		sizeof(socketAddress)) < 0) {
	        	perror("connect");
			exit(1);
	}
	return sock;
}

int sendMessage(char * host, int port, char * message, char * user, char * password, char * args, char * response) {
	int sock = open_client(host, port);

	write(sock, message, strlen(message));
	write(sock, " ", 1);
	write(sock, user, strlen(user));
	write(sock, " ", 1);
	write(sock, password, strlen(password));
	write(sock, " ", 1);
	write(sock, args, strlen(args));
	write(sock, "\r\n", 2);

	int i;
	int j = 0;
	for(i = read(sock, response + j, MAX_RESPONSE - j); i > 0; j += i);
	
	response[j] = '\0';
	close(sock);
}

char * get_messages() {
	char response[MAX_RESPONSE];
	char * room1 = strdup(args);
	char * room2 = strdup("0 ");
	strcat(room2, room1);
	sendMessage(host, port, "GET-MESSAGES", user, password, room2, response);
	printf("user = %s, room = %s, Response here is = %s\n",user, room2, response);
	if (!(strstr(response, "ERROR (User not in room)\r\n") != NULL) && !(strstr(response, "ERROR (Wrong password)\r\n") != NULL)) {
		return response;
	} else {
		gtk_label_set_text(GTK_LABEL(currentStatus),"Denied Getting Messages");
		return "";
	}
}

char* list_room()
{
	char temp[MAX_RESPONSE];
	sendMessage(host, port, "LIST-ROOMS", user, password, "", temp);
	char * temp2 = (char *) malloc(sizeof(temp) + 1);
	temp2 = strdup(temp);
	if (!(strstr(temp2, "DENIED\r\n") != NULL) && !(strstr(temp2, "ERROR (Wrong password)\r\n") != NULL)) {
		free(temp2);
		return temp;
	} else {
		free(temp2);
		gtk_label_set_text(GTK_LABEL(currentStatus),"Denied Listing Room(Wrong Pass)");
		return "";
	}
}

void update_list_rooms() {
    GtkTreeIter iter;
    int i;
    char * response = strdup(list_room());
    char *tok;
    if(roomChange) {
	tok = strtok (response,"\r\n");
	while (tok != NULL) {
		string stok(tok);
		roomVecNew.push_back(stok);
	        tok = strtok (NULL, "\r\n");
	}
	int rn = roomVecNew.size();
	int r = roomVec.size();
	int j;
	int k;
	int count = 0;
	for(j = 0; j < roomVecNew.size(); j++) {
		count = 0;
		for(k = 0; k < roomVec.size(); k++) {
			if(roomVecNew[j].compare(roomVec[k]) != 0) {
				count++;
			}
			if(count == roomVec.size()) {
				gchar *msg = g_strdup_printf (roomVecNew[j].c_str());
				gtk_list_store_append (GTK_LIST_STORE (list_rooms), &iter);
				gtk_list_store_set (GTK_LIST_STORE (list_rooms), &iter, 0, msg, -1);
				g_free(msg);
			}
		}
	}
	roomVec.swap(roomVecNew);
	roomVecNew.clear();
    } else {
	tok = strtok (response,"\r\n");
	while (tok != NULL) {
		gchar *msg = g_strdup_printf (tok);
		gtk_list_store_append (GTK_LIST_STORE (list_rooms), &iter);
		gtk_list_store_set (GTK_LIST_STORE (list_rooms), &iter, 0, msg, -1);
		g_free (msg);
		roomVec.push_back(tok);
		tok = strtok (NULL, "\r\n");
	}
	if(roomVec.size() > 0) {
		roomChange = true;
	}
    }
}

char * print_users_in_room() {
	char response[MAX_RESPONSE];
	sendMessage(host, port, "GET-USERS-IN-ROOM", user, password, args, response);
	if(strstr(response, "DENIED\r\n") != NULL) {
		printf("Denied Print User = %s\n", user);
		return "";
	} else {
		return response;
	}
}

/* Add some text to our text widget - this is a callback that is invoked
when our window is realized. We could also force our window to be
realized with gtk_widget_realize, but it would have to be part of
a hierarchy first */

static void insert_text( GtkTextBuffer *buffer, const char * initialText )
{
   GtkTextIter iter;
 
   gtk_text_buffer_get_end_iter(buffer, &iter);
   gtk_text_buffer_insert (buffer, &iter, initialText,-1);
}
   
static GtkWidget *create_text_User( const char * initialText )
{
	GtkWidget *scrolled_window;
	   
	GtkTextBuffer *buffer;

	viewUser = gtk_text_view_new ();
	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (viewUser));
	gtk_text_view_set_editable (GTK_TEXT_VIEW(viewUser), FALSE);
	scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_container_add (GTK_CONTAINER (scrolled_window), viewUser);
	insert_text (buffer, initialText);
	gtk_widget_show_all (scrolled_window);
	return scrolled_window;
}

void room_changed(GtkWidget * widget, gpointer text) {
	GtkTreeIter iter;
	GtkTreeModel *model;
	char *roomName;
	int i;
	GtkTextBuffer * buffer;
	vector<string> userRoomVec;
	if (gtk_tree_selection_get_selected(GTK_TREE_SELECTION(treeSel), &model, &iter)) {
		gtk_tree_model_get(model, &iter, 0, &roomName,  -1);
		gtk_label_set_text(GTK_LABEL(currentStatus), roomName);
		args = strdup(roomName);
		buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (viewUser));
		char * response2 = strdup(print_users_in_room());
		char * tok;
		tok = strtok (response2,"\r\n");
		while(tok != NULL) {
			string stok(tok); 
			userRoomVec.push_back(stok);
			tok = strtok (NULL, "\r\n");
		}
		response2 = strdup(print_users_in_room());
		roomUser = create_text_User(strdup(response2));
		gtk_table_attach_defaults (GTK_TABLE (table), roomUser, 4, 8, 1, 4);
		gtk_widget_show (roomUser);
		
		g_free(roomName);
	}
}

/* Create the list of "messages" */
static GtkWidget *create_list( const char * titleColumn, GtkListStore *model )
{
    GtkWidget *scrolled_window;
    GtkWidget *tree_view;
    //GtkListStore *model;
    GtkCellRenderer *cell;
    GtkTreeViewColumn *column;

    int i;
   
    /* Create a new scrolled window, with scrollbars only if needed */
    scrolled_window = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
				    GTK_POLICY_AUTOMATIC, 
				    GTK_POLICY_AUTOMATIC);
   
    //model = gtk_list_store_new (1, G_TYPE_STRING);
    tree_view = gtk_tree_view_new ();
    gtk_container_add (GTK_CONTAINER (scrolled_window), tree_view);
    gtk_tree_view_set_model (GTK_TREE_VIEW (tree_view), GTK_TREE_MODEL (model));
    gtk_widget_show (tree_view);
   
    cell = gtk_cell_renderer_text_new ();

    column = gtk_tree_view_column_new_with_attributes (titleColumn,
                                                       cell,
                                                       "text", 0,
                                                       NULL);
  
    gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view),
	  		         GTK_TREE_VIEW_COLUMN (column));

    return scrolled_window;
}
   
/* Create a scrolled text area that displays a "message" */

static GtkWidget *create_text( const char * initialText )
{
	GtkWidget *scrolled_window;
      
        GtkTextBuffer *buffer;

	view = gtk_text_view_new ();
	buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
	gtk_text_view_set_editable (GTK_TEXT_VIEW(view),FALSE);
	scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),GTK_POLICY_AUTOMATIC,GTK_POLICY_AUTOMATIC);

	gtk_container_add (GTK_CONTAINER (scrolled_window), view);
	insert_text (buffer, initialText);
	gtk_widget_show_all (scrolled_window);
	return scrolled_window;
}

void update_messages(GtkWidget *widget, gpointer text)
{
	GtkTreeIter iter;
	GtkTreeModel *tmodel;
	char *roomName;
	int i;
	GtkTextBuffer *buffer;
	vector<string> userRoomVec;
	if (gtk_tree_selection_get_selected(GTK_TREE_SELECTION(treeSel), &tmodel, &iter)) {
	        gtk_tree_model_get(tmodel, &iter, 0, &roomName,  -1);
		gtk_label_set_text(GTK_LABEL(currentStatus), roomName);
		args = strdup(roomName);
		buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
		char * response2 = strdup(get_messages());
		char * tok;
		tok = strtok (response2,"\r\n");
		while (tok != NULL) {
			string stok(tok);
			userRoomVec.push_back(stok);
			printf("Message: %s\n", tok);
			tok = strtok (NULL, "\r\n");
		}
		response2 = strdup(get_messages());
		messages_1 = create_text(strdup(response2));
		gtk_table_attach_defaults (GTK_TABLE (table), messages_1, 2, 10, 5, 11);
		gtk_widget_show (messages_1);
		g_free(roomName);
	}

}

static gboolean time_handler(GtkWidget * widget)
{
	if(widget->window == NULL) {
		return FALSE;
	}
	update_list_rooms();
	update_messages(widget, currentStatus);
	room_changed(widget,currentStatus);
	return TRUE;
}

GdkPixbuf * create_pixbuf(const gchar * filename)
{
	GdkPixbuf *pixbuf;
	GError *error = NULL;
	pixbuf = gdk_pixbuf_new_from_file(filename, &error);
	if (!pixbuf) {
	        fprintf(stderr, "%s\n", error->message);
		g_error_free(error);
	}

	return pixbuf;
}

static void entry_toggle_visibility( GtkWidget *checkbutton, GtkWidget *entry)
{
	gtk_entry_set_visibility (GTK_ENTRY (entry), GTK_TOGGLE_BUTTON (checkbutton)->active);
}



void send_msg()
{
	GtkWidget * widget;
	char response[MAX_RESPONSE];
	char * room;
	if(loggedIn) {
		if(strcmp(((char *) gtk_entry_get_text(GTK_ENTRY(messageEntry))),"") == 0) {
			gtk_label_set_text(GTK_LABEL(currentStatus), "No Message");
		} else {
			strcat(room, " ");
			strcat(room, (char *) gtk_entry_get_text(GTK_ENTRY(messageEntry)));
			strcat(room, (char *) "\n");
			sendMessage(host, port, "SEND-MESSAGE", user, password, room, response);
			if (strstr(response, "OK\r\n") != NULL) {
				update_messages(widget, currentStatus);
				gtk_label_set_text(GTK_LABEL(currentStatus), "Message Sent");
			}
		}
	} else {
		gtk_label_set_text(GTK_LABEL(currentStatus), "Not Logged In!");
	}
}



void login()
{
	loggedIn = true;
	g_timeout_add(5000, (GSourceFunc) time_handler, (gpointer) window);
	char temp[MAX_RESPONSE];
	user = (char*) gtk_entry_get_text(GTK_ENTRY(userName));
	password = (char *) gtk_entry_get_text(GTK_ENTRY(passWord));
	sendMessage(host, port, "LOG-IN", user, password, "", temp);
	if (strstr(temp, "OK\r\n") != NULL) {
		gtk_label_set_text(GTK_LABEL(currentStatus),"Logged In");
		list_room();
		update_list_rooms();
	} else {
		gtk_label_set_text(GTK_LABEL(currentStatus),"Incorrect Login");
	}
}

void signup(GtkWidget * widget, gpointer data) 
{
	char temp[MAX_RESPONSE];
	user = (char *) gtk_entry_get_text(GTK_ENTRY(userName));
	password = (char *) gtk_entry_get_text(GTK_ENTRY(passWord));
	sendMessage(host, port, "ADD-USER", user, password, "", temp);
	if(strstr(temp, "OK\r\n") != NULL) {
		gtk_label_set_text(GTK_LABEL(currentStatus),"Signed Up");
	} else {
		gtk_label_set_text(GTK_LABEL(currentStatus),"Username Already Taken");
	}
}

void create_room()
{
	char temp[MAX_RESPONSE];
	args = (char *) gtk_entry_get_text(GTK_ENTRY(entryRoom));
	sendMessage(host, port, "CREATE-ROOM", user, password, args, temp);
	if (strstr(temp, "OK\r\n") != NULL) {
		update_list_rooms();
		gtk_label_set_text(GTK_LABEL(currentStatus),"Room Created");
	}
}

void enter_room()
{
	GtkWidget * widget;
	char temp[MAX_RESPONSE];
	sendMessage(host, port, "ENTER-ROOM", user, password, args, temp);
	if(strstr(temp, "OK\r\n") != NULL) {
		room_changed(widget,currentStatus);
		gtk_label_set_text(GTK_LABEL(currentStatus), "Entered Room");
	}
}

void leave_room() 
{
	GtkWidget * widget;
	char response[MAX_RESPONSE];
	sendMessage(host, port, "ENTER-ROOM", user, password, args, response);
	if(strstr(response, "OK\r\n") != NULL) {
		gtk_label_set_text(GTK_LABEL(currentStatus), "Left Room");
		room_changed(widget,currentStatus);
	}
}

int main( int   argc,
          char *argv[] )
{
    GtkWidget *window;
    GtkWidget *list;
    GtkWidget *messages;
    GtkWidget *myMessage;

    gtk_init (&argc, &argv);
   
    window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title (GTK_WINDOW (window), "Paned Windows");
    g_signal_connect (window, "destroy",
	              G_CALLBACK (gtk_main_quit), NULL);
    gtk_container_set_border_width (GTK_CONTAINER (window), 10);
    gtk_widget_set_size_request (GTK_WIDGET (window), 450, 400);

    // Create a table to place the widgets. Use a 7x4 Grid (7 rows x 4 columns)
    GtkWidget *table = gtk_table_new (7, 4, TRUE);
    gtk_container_add (GTK_CONTAINER (window), table);
    gtk_table_set_row_spacings(GTK_TABLE (table), 5);
    gtk_table_set_col_spacings(GTK_TABLE (table), 5);
    gtk_widget_show (table);

    // Add list of rooms. Use columns 0 to 4 (exclusive) and rows 0 to 4 (exclusive)
    list_rooms = gtk_list_store_new (1, G_TYPE_STRING);
    update_list_rooms();
    list = create_list ("Rooms", list_rooms);
    gtk_table_attach_defaults (GTK_TABLE (table), list, 2, 4, 0, 2);
    gtk_widget_show (list);
   
    // Add messages text. Use columns 0 to 4 (exclusive) and rows 4 to 7 (exclusive) 
    messages = create_text ("Peter: Hi how are you\nMary: I am fine, thanks and you?\nPeter: Fine thanks.\n");
    gtk_table_attach_defaults (GTK_TABLE (table), messages, 0, 4, 2, 5);
    gtk_widget_show (messages);
    // Add messages text. Use columns 0 to 4 (exclusive) and rows 4 to 7 (exclusive) 

    myMessage = create_text ("I am fine, thanks and you?\n");
    gtk_table_attach_defaults (GTK_TABLE (table), myMessage, 0, 4, 5, 7);
    gtk_widget_show (myMessage);

    // Add send button. Use columns 0 to 1 (exclusive) and rows 4 to 7 (exclusive)
    GtkWidget *send_button = gtk_button_new_with_label ("Send");
    gtk_table_attach_defaults(GTK_TABLE (table), send_button, 0, 1, 7, 8); 
    gtk_widget_show (send_button);
    
    gtk_widget_show (table);
    gtk_widget_show (window);

    gtk_main ();

    return 0;
}

