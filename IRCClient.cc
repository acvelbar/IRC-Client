
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
#include <sstream>

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
int port = 3052;
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
	if(args != NULL) {
		write(sock, args, strlen(args));
	}	
	write(sock, "\r\n", 2);

	int j = 0;
	int i = read(sock, response + j, MAX_RESPONSE - j);
	while(i > 0) {
		j += i;
		i = read(sock, response + j, MAX_RESPONSE - j);
	}
	
	response[j] = '\0';
	printf("RECIEVED: %s\n", response);
	close(sock);
}

char * get_messages() {
	char response[MAX_RESPONSE];
	char * room1 = strdup(args);
	char * room2 = strdup("-1 ");
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
		char * response2 = print_users_in_room();
		char * tok;
		tok = strtok (response2,"\r\n");
		while(tok != NULL) {
			string stok(tok); 
			userRoomVec.push_back(stok);
			tok = strtok (NULL, "\r\n");
		}
		response2 = print_users_in_room();
		roomUser = create_text_User(strdup(response2));
		gtk_table_attach_defaults (GTK_TABLE (table), roomUser, 4, 8, 1, 4);
		gtk_widget_show (roomUser);
		
		//free(response2);
		g_free(roomName);
	}
}

/* Create the list of "messages" */
static GtkWidget *create_list( const char * titleColumn, GtkListStore *model )
{
    GtkWidget *scrolled_window;
    //GtkWidget *tree_view;
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
	gchar *roomName;
	int i;
	GtkTextBuffer *buffer;
	vector<string> userRoomVec;
	if (gtk_tree_selection_get_selected(treeSel, &tmodel, &iter)) {
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
		
		string resp3;
		stringstream ss(response2);
		string to;
		while(getline(ss, to, '\n')) {
			to = to.substr(to.find(" ") + 1);
			resp3 += to.substr(0, to.find(" "));
			resp3 += ":    ";
			resp3 += to.substr(to.find(" ") + 1);

		}

		
		messages_1 = create_text(strdup(resp3.c_str()));
		gtk_table_attach_defaults (GTK_TABLE(table), messages_1, 2, 10, 5, 11);
		gtk_widget_show (messages_1);
		g_free(roomName);
	} else {
		printf("No selection\n");
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
//	GtkWidget * widget;
	char response[MAX_RESPONSE];
	char * room = strdup(args);
	if(loggedIn) {
		if(strcmp(((char *) gtk_entry_get_text(GTK_ENTRY(messageEntry))),"") == 0) {
			gtk_label_set_text(GTK_LABEL(currentStatus), "No Message");
		} else {
			strcat(room, " ");
			strcat(room, (char *) gtk_entry_get_text(GTK_ENTRY(messageEntry)));
			strcat(room, (char *) "\n");
			sendMessage(host, port, "SEND-MESSAGE", user, password, room, response);
			if (strstr(response, "OK\r\n") != NULL) {
				printf("Displaying message");
//				update_messages(widget, currentStatus);
				gtk_label_set_text(GTK_LABEL(currentStatus), "Message Sent");
			}
		}
	} else {
		gtk_label_set_text(GTK_LABEL(currentStatus), "Not Logged In!");
	}
}


void send_msg2(char * msg)
{
	char response[MAX_RESPONSE];
	char * room = strdup(args);
	if(loggedIn) {
		strcat(room, " ");
		strcat(room, msg);
		strcat(room, (char *) "\n");
		sendMessage(host, port, "SEND-MESSAGE", user, password, room, response);
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
	sendMessage(host, port, "ADD-USER", user, password, "", temp);
	if (strstr(temp, "OK\r\n") != NULL) {
		gtk_label_set_text(GTK_LABEL(currentStatus),"Logged In");
		list_room();
		update_list_rooms();
	} else {
		gtk_label_set_text(GTK_LABEL(currentStatus),"Incorrect Login");
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

	//	string buffer1 = user;
	//	buffer1 += " entered room";
	//	send_msg2((char *) buffer1.c_str());
		string b1 = args;
		b1 += " ";
		b1 += user;
		b1 += " entered room.";
		sendMessage(host, port, "SEND-MESSAGE", user, password, (char *) b1.c_str(), temp);
	}
}

void leave_room() 
{
	GtkWidget * widget;
	char response[MAX_RESPONSE];
	
//	string buffer1 = user;
//	buffer1 += " left room";
//	send_msg2((char *) buffer1.c_str());

	string b1 = args;
	b1 += " ";
	b1 += user;
	b1 += " left room.";
	sendMessage(host, port, "SEND-MESSAGE", user, password, (char *) b1.c_str(), response);

	sendMessage(host, port, "LEAVE-ROOM", user, password, args, response);
	if(strstr(response, "OK\r\n") != NULL) {
		gtk_label_set_text(GTK_LABEL(currentStatus), "Left Room");
		room_changed(widget,currentStatus);	
	}
}

int main( int   argc,
          char *argv[] )
{
    GtkWidget *userList;
    GtkWidget *list;
    GtkWidget *myMessage;
    GtkWidget *frame;
    GtkWidget *entry;
    GdkColor color;
    GdkColor color2;
    GdkColor color3;
    GtkWidget *status;
    GtkWidget *labelMsg;
    GtkWidget *labelPass;
    GtkWidget *labelRoom;
    GtkWidget *labelUser;
    GtkWidget *labelUserRoom;
    //GtkWidget *table;

    tree_view = gtk_tree_view_new ();//FIX TREEVIEW
    gtk_init (&argc, &argv);
    loggedIn = false;
    window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);
    gtk_container_set_border_width (GTK_CONTAINER (window), 10);
    gtk_widget_set_size_request (GTK_WIDGET (window), 650, 550);
    gdk_color_parse ("#4D5E5F", &color2);
    gtk_widget_modify_bg (GTK_WIDGET(window), GTK_STATE_NORMAL, &color2);

    // Create a table to place the widgets. Use a 7x4 Grid (7 rows x 4 columns)
    table = gtk_table_new (14, 12, TRUE);
    gtk_container_add (GTK_CONTAINER (window), table);
    gtk_table_set_row_spacings(GTK_TABLE (table), 10);
    gtk_table_set_col_spacings(GTK_TABLE (table), 10);
    gtk_widget_show (table);

    // Add list of rooms. Use columns 0 to 4 (exclusive) and rows 0 to 4 (exclusive)
    list_rooms = gtk_list_store_new (1, G_TYPE_STRING);
    //update_list_rooms();
    list = create_list ("Rooms", list_rooms);
    gtk_table_attach_defaults (GTK_TABLE (table), list, 0, 4, 0, 5);
    gtk_widget_show (list);
    gtk_table_set_homogeneous(GTK_TABLE(table), TRUE);

    labelMsg = gtk_label_new("Messages:");
    gtk_misc_set_alignment(GTK_MISC(labelMsg),0.0,0.5);
    gtk_table_attach(GTK_TABLE (table), labelMsg,4, 8, 4, 5, GTK_FILL, GTK_FILL, 0, 0);
    gtk_widget_show(labelMsg);

    // Add messages text. Use columns 0 to 4 (exclusive) and rows 4 to 7 (exclusive) 
    roomUser = create_text_User("");
    gtk_table_attach_defaults(GTK_TABLE(table), roomUser, 4, 8, 1, 4);
    gtk_widget_show(roomUser);

    labelUserRoom = gtk_label_new("Users In Room:");
    gtk_misc_set_alignment(GTK_MISC(labelUserRoom),0.0,0.5);
    gtk_table_attach(GTK_TABLE (table), labelUserRoom,4, 8, 0, 1, GTK_FILL, GTK_FILL, 0, 0);
    gtk_widget_show(labelUserRoom);
    //720

    messages_1 = create_text ("");
    gtk_table_attach_defaults (GTK_TABLE (table), messages_1, 2, 10, 5, 11);
    gtk_widget_show (messages_1);

    entryRoom = gtk_entry_new_with_max_length(0);
    gtk_table_attach_defaults (GTK_TABLE (table), entryRoom, 0, 2, 6, 7);
    gtk_widget_show(entryRoom);
    
    labelRoom = gtk_label_new("Room name:");
    gtk_misc_set_alignment(GTK_MISC(labelRoom),0.0,0.5);
    gtk_table_attach(GTK_TABLE (table), labelRoom,0, 2, 5, 6, GTK_FILL, GTK_FILL, 0, 0);
    gtk_widget_show(labelRoom);
    gdk_color_parse ("black", &color3);
    gtk_widget_modify_bg (GTK_WIDGET(labelRoom), GTK_STATE_NORMAL, &color3);

    GtkWidget *send_button = gtk_button_new_with_label ("Send");
    gtk_table_attach_defaults(GTK_TABLE (table), send_button, 2, 4, 13, 14); 
    gdk_color_parse ("#F84934", &color);
    gtk_widget_modify_bg (GTK_WIDGET(send_button), GTK_STATE_NORMAL, &color);
    g_signal_connect (send_button, "clicked", G_CALLBACK (send_msg), NULL);
    gtk_widget_show (send_button); 
    messageEntry = gtk_entry_new_with_max_length(0);
    gtk_table_attach_defaults (GTK_TABLE (table), messageEntry, 2, 10, 11, 13);
    gtk_widget_show(messageEntry);

    GtkWidget *create_room2 = gtk_button_new_with_label ("Create Room");
    gtk_table_attach_defaults(GTK_TABLE (table), create_room2, 0, 2, 7, 8); 
    gtk_widget_modify_bg (GTK_WIDGET(create_room2), GTK_STATE_NORMAL, &color); 
    gtk_widget_show (create_room2);
    g_signal_connect (create_room2, "clicked", G_CALLBACK (create_room), NULL);

    GtkWidget *enter_room_Btn = gtk_button_new_with_label ("Enter Room");
    gtk_table_attach_defaults(GTK_TABLE (table), enter_room_Btn, 0, 2, 8, 9); 
    gdk_color_parse ("#F84934", &color);
    gtk_widget_modify_bg (GTK_WIDGET(enter_room_Btn), GTK_STATE_NORMAL, &color);
    gtk_widget_show (enter_room_Btn); 
    g_signal_connect (enter_room_Btn, "clicked", G_CALLBACK (enter_room), NULL);

    GtkWidget *leave_room_Btn = gtk_button_new_with_label ("Leave Room");
    gtk_table_attach_defaults(GTK_TABLE (table), leave_room_Btn, 0, 2, 9, 10); 
    gtk_widget_modify_bg (GTK_WIDGET(leave_room_Btn), GTK_STATE_NORMAL, &color);
    gtk_widget_show (leave_room_Btn); 
    g_signal_connect (leave_room_Btn, "clicked", G_CALLBACK (leave_room), NULL);
    
    GtkWidget *login_button = gtk_button_new_with_label ("Log In");
    gtk_table_attach_defaults(GTK_TABLE (table), login_button, 10, 12, 12, 13);
    
    gtk_widget_modify_bg (GTK_WIDGET(login_button), GTK_STATE_NORMAL, &color);
    gtk_widget_show (login_button);
    g_signal_connect (login_button, "clicked", G_CALLBACK (login), NULL);

/*    GtkWidget *signup_button = gtk_button_new_with_label ("Signup");
    gtk_table_attach_defaults(GTK_TABLE (table), signup_button, 10, 12, 13, 14);
    gtk_widget_modify_bg (GTK_WIDGET(signup_button), GTK_STATE_NORMAL, &color);
    gtk_widget_show (signup_button);  
    g_signal_connect (signup_button, "clicked", G_CALLBACK (signup), (gpointer) "Signup");*/
	
    labelRoom = gtk_label_new("Enter User Name:");
    gtk_misc_set_alignment(GTK_MISC(labelRoom),0.0,0.5);
    gtk_table_attach(GTK_TABLE (table), labelRoom, 9, 11, 5, 7, GTK_FILL, GTK_FILL, 0, 0);

    userName = gtk_entry_new_with_max_length(0);
    gtk_table_attach_defaults (GTK_TABLE (table), userName, 10, 12, 7, 9);
    gtk_widget_show(userName);
	   
    labelUser = gtk_label_new("Username:");
    gtk_misc_set_alignment(GTK_MISC(labelUser),0.0,0.5);
    gtk_table_attach(GTK_TABLE (table), labelUser,10, 12, 7, 8, GTK_FILL, GTK_FILL, 0, 0);
    gtk_widget_show(labelUser);

    labelPass = gtk_label_new("Password:");
    gtk_misc_set_alignment(GTK_MISC(labelPass),0.0,0.5);
    gtk_table_attach(GTK_TABLE (table), labelPass,10, 12, 9, 10, GTK_FILL, GTK_FILL, 0, 0);
    gtk_widget_show(labelPass);

    passWord = gtk_entry_new_with_max_length(0);
    gtk_table_attach_defaults (GTK_TABLE (table), passWord, 10, 12, 9, 11);
    gtk_widget_show(passWord);

    GtkWidget *check = gtk_check_button_new_with_label ("Editable");
    g_signal_connect (check, "toggled", G_CALLBACK (entry_toggle_visibility), passWord);
    //GtkWidget *image = gtk_image_new_from_file("new-user-image-default.png");
    //gtk_table_attach_defaults(GTK_TABLE (table), image, 10, 12, 0, 5); 
    //gtk_widget_show (image);

    status = gtk_label_new("Status:");
    gtk_misc_set_alignment(GTK_MISC(status),0.0,0.5);
    gtk_table_attach(GTK_TABLE (table), status,8, 9, 1, 3, GTK_FILL, GTK_FILL, 0, 0);
    gtk_widget_show(status);

    currentStatus = gtk_label_new("Login/Signup");
    gtk_misc_set_alignment(GTK_MISC(currentStatus),0.0,0.5);
    gtk_table_attach(GTK_TABLE (table), currentStatus,8, 11, 2, 4, GTK_FILL, GTK_FILL, 0, 0);
    gtk_widget_show(currentStatus);

    
    treeSel = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree_view));
    g_signal_connect(treeSel, "changed",  G_CALLBACK(room_changed), currentStatus); 

    gtk_widget_show (table);
    gtk_widget_show (window);
    gtk_widget_hide (window);
    gtk_widget_show (window);
    gtk_window_set_title(GTK_WINDOW(window), "IRCClient");

    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
    //gtk_window_set_icon(GTK_WINDOW(window), create_pixbuf("chat_pic.png"));

    gtk_main ();
    
    return 0;
}

