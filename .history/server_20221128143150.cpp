#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <fcntl.h>
#include <cassert>
#include <sstream>
#include <iostream>
#include <string>
#include <cstring>
#include <algorithm>
#include <exception>
#include <map>
#include <vector>

#define PORT 8888
#define MAXLINE 1024
#define MAXCLIENT 15

using namespace std;

struct user{
    string username;
    string email;
    string password;
    int fd;
    int login;
    string room;
    vector<struct invitation>invitation;
};

struct room{
    string id;
    string manager;
    string invitation_code;
    int status;
    int rounds;
    string guess_number;
    vector<string>player;
    int current_player;
};

struct invitation{
    string inviter;
    string inviter_email;
    string room_id;
    string invitation_code;
};

vector<user> users;
vector<room> rooms;

int checkName(string username){
    int result = -1;
    for(int i = 0; i < users.size(); i++){
        if(users[i].username == username){
            result = i;
            break;
        }
    }
    return result;
}

int checkEmail(string email){
    int result = -1;
    for(int i = 0; i < users.size(); i++){
        if(users[i].email == email){
            result = i;
            break;
        }
    }
    return result;
}

int checkRoom(string id){
    int result = -1;
    for(int i = 0; i < rooms.size(); i++){
        if(rooms[i].id == id){
            result = i;
            break;
        }
    }
    return result;
}

int checkInviteRepeat(int u_index, string inviter, string room_id){ 
    int result = -1;
    for(int i = 0; i < users[u_index].invitation.size(); i++){
        if(users[u_index].invitation[i].inviter == inviter && users[u_index].invitation[i].room_id == room_id){
            result = i;
            break;
        }
    }
    return result;
}

int checkInvite(struct user u,string inviter_email){
    int result = -1;
    for(int i = 0; i < u.invitation.size(); i++){
        if(u.invitation[i].inviter_email == inviter_email){
            result = i;
            break;
        }
    }
    return result;
}

int findPlayerIndex(int r_index, string player){ 
    int result = -1;
    for(int i = 0; i < rooms[r_index].player.size(); i++){
        if(rooms[r_index].player[i] == player){
            result = i;
            break;
        }
    }
    return result;
}


bool room_compare(room a, room b){
    char *at = a.id.data();
    char *bt = b.id.data();
    int aa = atoi(at);
    int bb = atoi(bt);
    if(aa < bb){
        return 1; 
    }
    else{
        return 0;
    }
}
bool user_compare(user a, user b){
    return a.username < b.username;
}
bool invitation_compare(invitation a, invitation b){
    char *at = a.room_id.data();
    char *bt = b.room_id.data();
    int aa = atoi(at);
    int bb = atoi(bt);
    if(aa < bb){
        return 1; 
    }
    else{
        return 0;
    }
}


int main(int argc , char *argv[]){
    struct sockaddr_in cliaddr;
	struct sockaddr_in address;
    socklen_t len;
	fd_set readfds;

	int master_socket, new_socket; //socket initialize
    pair<int, string> client_socket[MAXCLIENT];
    int opt = 1, activity, valread, length;
	int sd, max_sd;
	char buffer[MAXLINE];
    int gamemode = 0;
    int guesstime = 0;
    char answer[MAXLINE];

    for(int i = 0; i < MAXCLIENT; i++ ){
        pair<int, string> temp;
        temp.first = 0;
        temp.second = "";
    }

    // create TCP socket
	if((master_socket = socket(AF_INET , SOCK_STREAM , 0)) == 0){
		perror("create socket failed");
		exit(EXIT_FAILURE);
	}
	
	if(setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt,sizeof(opt)) < 0 ){
		perror("set sockopt failed");
		exit(EXIT_FAILURE);
	}
	
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons( PORT );
    
	//bind the socket to localhost port 8888
	bind(master_socket, (struct sockaddr *)&address, sizeof(address));
	//printf("Listener on port %d \n", PORT);

    // create UDP socket
    int udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
    bind(udp_socket, (struct sockaddr*)&address, sizeof(address));

	if (listen(master_socket, MAXCLIENT) < 0){
		perror("listen failed");
		exit(EXIT_FAILURE);
	}
		
	int addrlen = sizeof(address);
	
	while(1){
		FD_ZERO(&readfds);
		FD_SET(master_socket, &readfds);
        FD_SET(udp_socket, &readfds);
        max_sd = max(master_socket, udp_socket);
		for(int i = 0 ; i < MAXCLIENT ; i++){
			sd = client_socket[i].first;
			if(sd > 0){
                FD_SET( sd ,&readfds);
            }
			if(sd > max_sd){
                max_sd = sd;
            }
		}
		activity = select(max_sd + 1 ,&readfds, NULL, NULL, NULL);
		if((activity < 0) && (errno!=EINTR)){
			printf("select failed");
		} 
        //udp
        if(FD_ISSET(udp_socket, &readfds)){
            len = sizeof(cliaddr);
            memset(&buffer, 0, sizeof(buffer));
            recvfrom(udp_socket, buffer, sizeof(buffer), 0, (struct sockaddr*)&cliaddr, &len);
            string tt;
            stringstream ss(buffer); 
            char** arg = new char*[500];
            length = 0;
            while (getline(ss, tt, ' ')) {
                char * temp_char = new char[tt.size()+1];
                copy(tt.begin(), tt.end(), temp_char);
                temp_char[tt.size()] = '\0';
                arg[length] = temp_char;
                length++;
            }
            for(int i = 0 ;i < 20; i++){
                if(arg[length-1][i] == '\n'){
                    arg[length-1][i] = '\0';
                    break;
                }
            }
            arg[length] = NULL;
            string data;
            if(!strcmp(arg[0], "register")){
                if(length != 4){
                    data = "Usage: register <username> <email> <password>\n";
                }
                else{
                    if(checkName(arg[1]) == -1 && checkEmail(arg[2]) == -1){
                        user new_user;
                        string arg1 = arg[1];
                        string arg2 = arg[2];
                        string arg3 = arg[3];
                        new_user.username = arg1;
                        new_user.email = arg2;
                        new_user.password = arg3;
                        new_user.fd = -1;
                        new_user.login = 0;
                        new_user.room = "none";
                        users.push_back(new_user);
                        data = "Register Successfully\n";
                    }
                    else{
                        data = "Username or Email is already used\n";
                    }
                }
                sendto(udp_socket, data.c_str(), data.length(), 0, (struct sockaddr*)&cliaddr, sizeof(cliaddr)); 
            }
            else if(!strcmp(arg[0], "list") && !strcmp(arg[1], "rooms")){
                int data_length;
                if(rooms.size() == 0){
                    data = "List Game Rooms\nNo Rooms\n";
                }
                else{
                    data = "List Game Rooms\n";
                    vector<room> temp_room = rooms;
                    sort(temp_room.begin(), temp_room.end(), room_compare);
                    for(int i = 0; i < temp_room.size(); i++){
                        string p1, p3;
                        if(temp_room[i].invitation_code == "none"){
                            p1 = "Public";
                        }
                        else{
                            p1 = "Private";
                        }
                        if(temp_room[i].status == 0){
                            p3 = " is open for players\n";
                        }
                        else{
                            p3 = " has started playing\n";
                        }
                        data = data + to_string(i+1) + ". (" + p1 + ") Game Room " + temp_room[i].id + p3;
                    }
                }
                sendto(udp_socket, data.c_str(), data.length(), 0, (struct sockaddr*)&cliaddr, sizeof(cliaddr));
            }
            else if(!strcmp(arg[0], "list") && !strcmp(arg[1], "users")){
                if(users.size()==0){
                    data = "List Users\nNo Users\n";
                }else{
                    vector<user> temp_uesr = users;
                    sort(temp_uesr.begin(), temp_uesr.end(), user_compare);
                    data = "List Users\n";
                    for(int i = 0; i < temp_uesr.size(); i++){
                        string p3;
                        if(temp_uesr[i].login == 0){
                            p3 = "> Offline\n";
                        }
                        else if(temp_uesr[i].login == 1){
                            p3 = "> Online\n";
                        }
                        data = data + to_string(i+1) + ". " + temp_uesr[i].username + "<" + temp_uesr[i].email + p3;
                    }
                }

                sendto(udp_socket, data.c_str(), data.length(), 0, (struct sockaddr*)&cliaddr, sizeof(cliaddr));
            }
        }
		//tcp first connect
		if(FD_ISSET(master_socket, &readfds)){ 
			if((new_socket = accept(master_socket, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0){
				perror("accept failed");
				exit(EXIT_FAILURE);
			}
			for(int i = 0; i < MAXCLIENT; i++){
				if(client_socket[i].first == 0){
					client_socket[i].first = new_socket;
					break;
				}
			}
		}
		//tcp	
		for(int i = 0; i < MAXCLIENT; i++){
			sd = client_socket[i].first;
			if(FD_ISSET(sd , &readfds)){
				memset(buffer,0,sizeof(buffer));
				if((valread = read(sd, buffer, MAXLINE)) == 0){
					getpeername(sd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
                    int u_index = checkName(client_socket[i].second);
                    int r_index = checkRoom(users[u_index].room);
                    users[u_index].login = 0;
                    users[u_index].fd = -1;
                    client_socket[i].first = 0;
					client_socket[i].second = "";
                    if(users[u_index].room != "none"){
                        if(rooms[r_index].manager == users[u_index].username){ //room manager
                            for(int i = 0; i < users.size(); i++){ // in this room
                                if(users[i].room == rooms[r_index].id){
                                    users[i].room = "none";
                                }
                            }
                            for(int i = 0; i < users.size(); i++){ // all users
                                for(int j = (users[i].invitation.size()-1); j >= 0; j--){ //all invitation
                                    if(users[i].invitation[j].inviter == users[u_index].username && users[i].invitation[j].room_id == rooms[r_index].id){
                                        users[i].invitation.erase(users[i].invitation.begin()+j);
                                    }
                                }
                            }
                            rooms.erase(rooms.begin()+r_index);
                        }
                        else{
                            int p_index = findPlayerIndex(r_index, users[u_index].username);
                            rooms[r_index].status = 0;
                            rooms[r_index].player.erase(rooms[r_index].player.begin()+p_index);
                            rooms[r_index].current_player = 0;
                        }
                        users[u_index].room = "none";
                    }
                    close(sd);
				}
				else{
                    len = sizeof(cliaddr);
                    string tt;
                    stringstream ss(buffer); 
                    char** arg = new char*[500];
                    length = 0;
                    while (getline(ss, tt, ' ')) {
                        char * temp_char = new char[tt.size()+1];
                        copy(tt.begin(), tt.end(), temp_char);
                        temp_char[tt.size()] = '\0';
                        arg[length] = temp_char;
                        length++;
                    }
                    for(int i = 0 ;i < 20; i++){
                        if(arg[length-1][i] == '\n'){
                            arg[length-1][i] = '\0';
                            break;
                        }
                    }
                    arg[length] = NULL;
                    string data;
                    if(!strcmp(arg[0], "login")){
                        if(length!=3){
                            data = "Usage: login <username> <password>";
                        }
                        else{
                            string arg1 = arg[1];
                            string arg2 = arg[2];
                            int u_index = checkName(arg1);
                            if(u_index == -1){
                                data = "Username does not exist\n";
                            }
                            else{
                                if(client_socket[i].second != ""){
                                    data = "You already logged in as " + client_socket[i].second + "\n";
                                }
                                else if(users[u_index].login == 1){
                                    data = "Someone already logged in as " + users[u_index].username + "\n";
                                }
                                else if(users[u_index].password != arg2){
                                    data = "Wrong password\n";
                                }
                                else{
                                    users[u_index].fd = sd;
                                    users[u_index].login = 1;
                                    client_socket[i].second = users[u_index].username;
                                    data = "Welcome, " + users[u_index].username + "\n";
                                }
                            }
                        }
                        sendto(sd, data.c_str(), strlen(data.c_str()),0, (const struct sockaddr*)&address,sizeof(sd));
                    }
                    else if(!strcmp(arg[0], "logout")){
                        int u_index = checkName(client_socket[i].second);
                        if(client_socket[i].second == ""){
                            data = "You are not logged in\n";
                        }
                        else if(users[u_index].room != "none"){
                            data = "You are already in game room " + users[u_index].room + ", please leave game room\n";
                        }
                        else{
                            users[u_index].fd = -1;
                            users[u_index].login = 0;
                            client_socket[i].second = "";
                            data = "Goodbye, " + users[u_index].username + "\n";
                        }
                        sendto(sd, data.c_str(), strlen(data.c_str()), 0, (const struct sockaddr*)&address, sizeof(sd));
                    }
                    else if(!strcmp(arg[0], "create")){
                        int u_index = checkName(client_socket[i].second);
                        string room_id = arg[3];
                        string code = "none";
                        if(!strcmp(arg[1], "private")){
                            code = arg[4];
                        }
                        if(client_socket[i].second == ""){
                            data = "You are not logged in\n";
                        }
                        else if(users[u_index].room != "none"){
                            data = "You are already in game room " + users[u_index].room + ", please leave game room\n";
                        }
                        else if(checkRoom(room_id) != -1){
                            data = "Game room ID is used, choose another one\n"; 
                        }
                        else{
                            room new_room;
                            new_room.id = room_id;
                            new_room.manager = users[u_index].username;
                            new_room.invitation_code = code;
                            new_room.status = 0;
                            new_room.player.push_back(users[u_index].username);
                            new_room.current_player = 0;
                            rooms.push_back(new_room);
                            users[u_index].room = room_id;
                            if(!strcmp(arg[1], "private")){
                                data = "You create private game room "+ room_id + "\n";
                            }
                            else if(!strcmp(arg[1], "public")){
                                data = "You create public game room "+ room_id + "\n";
                            }
                        }
                        sendto(sd, data.c_str(), strlen(data.c_str()),0, (const struct sockaddr*)&address,sizeof(sd));
                    }                    
                    else if(!strcmp(arg[0],"join")){
                        string room_id = arg[2];
                        int u_index = checkName(client_socket[i].second);
                        int r_index = checkRoom(room_id);
                        if(client_socket[i].second == ""){
                            data = "You are not logged in\n";
                        }
                        else if(users[u_index].room != "none"){
                            data = "You are already in game room " + users[u_index].room + ", please leave game room\n";
                        }
                        else if(r_index == -1){
                            data = "Game room " + room_id + " is not exist\n"; 
                        }
                        else if(rooms[r_index].invitation_code != "none"){
                            data = "Game room is private, please join game by invitation code\n"; 
                        }
                        else if(rooms[r_index].status == 1){
                            data = "Game has started, you can't join now\n"; 
                        }
                        else{
                            data = "You join game room "+ room_id + "\n"; 
                            users[u_index].room = room_id;
                            rooms[r_index].player.push_back(users[u_index].username);
                            for(int i = 0; i < users.size(); i++){
                                if(users[i].fd != sd && users[i].room == room_id){
                                    string welcome_data = "Welcome, " + users[u_index].username + " to game!\n";
                                    sendto(users[i].fd, welcome_data.c_str(), strlen(welcome_data.c_str()),0, (const struct sockaddr*)&address,sizeof(users[i].fd));
                                }
                            }
                        }
                        sendto(sd, data.c_str(), strlen(data.c_str()),0, (const struct sockaddr*)&address,sizeof(sd));
                    }
                    else if(!strcmp(arg[0],"invite")){
                        string mail = arg[1];
                        int u_index = checkName(client_socket[i].second);
                        int invitee_index = checkEmail(mail);
                        int r_index = checkRoom(users[u_index].room);
                        if(client_socket[i].second == ""){
                            data = "You are not logged in\n";
                        }
                        else if(users[u_index].room == "none"){
                            data = "You did not join any game room\n";
                        }
                        else if(rooms[r_index].manager != client_socket[i].second || rooms[r_index].invitation_code == "none"){ //not manager or public room
                            data = "You are not private game room manager\n"; 
                        }
                        else if(users[invitee_index].login == 0){
                            data = "Invitee not logged in\n";
                        }
                        else{
                            data = "You send invitation to " + users[invitee_index].username + "<" + mail + ">\n"; 
                            
                            string invitee_data = "You receive invitation from " + users[u_index].username + "<" + users[u_index].email + ">\n"; 
                            sendto(users[invitee_index].fd, invitee_data.c_str(), strlen(invitee_data.c_str()), 0, (const struct sockaddr*)&address, sizeof(users[invitee_index].fd));
                            if(checkInviteRepeat(invitee_index, users[u_index].username, users[u_index].room) == -1){
                                struct invitation new_invite;
                                new_invite.inviter = users[u_index].username;
                                new_invite.inviter_email = users[u_index].email;
                                new_invite.room_id = users[u_index].room;
                                new_invite.invitation_code = rooms[r_index].invitation_code;
                                users[invitee_index].invitation.push_back(new_invite);
                            }
                        }
                        sendto(sd, data.c_str(), strlen(data.c_str()),0, (const struct sockaddr*)&address,sizeof(sd));
                    }
                    else if(!strcmp(arg[0],"list") && !strcmp(arg[1],"invitations")){  
                        int u_index = checkName(client_socket[i].second);
                        if(client_socket[i].second == ""){
                            data = "You are not logged in\n";
                        }
                        else if(users[u_index].invitation.size() == 0){
                            data = "List invitations\nNo Invitations\n";                            
                        }
                        else{
                            data = "List invitations\n";
                            vector<struct invitation> temp_invite = users[u_index].invitation;
                            sort(temp_invite.begin(), temp_invite.end(), invitation_compare);
                            for(int i = 0; i < temp_invite.size(); i++){
                                data = data + to_string(i+1) + ". " + temp_invite[i].inviter + "<" + temp_invite[i].inviter_email + "> invite you to join game room " + temp_invite[i].room_id + ", invitation code is " + temp_invite[i].invitation_code + "\n";
                            }
                        }
                        sendto(sd, data.c_str(), strlen(data.c_str()),0, (const struct sockaddr*)&address,sizeof(sd));
                    }
                    else if(!strcmp(arg[0],"accept")){
                        string inviter_email = arg[1];
                        string code = arg[2];
                        int u_index = checkName(client_socket[i].second);
                        int inviter_index = checkEmail(inviter_email);
                        int r_index = checkRoom(users[inviter_index].room);
                        int invite_index = checkInvite(users[u_index], inviter_email);
                        if(client_socket[i].second == ""){
                            data = "You are not logged in\n";
                        }
                        else if(users[u_index].room != "none"){
                            data = "You are already in game room " + users[u_index].room + ", please leave game room\n";
                        }
                        else if(invite_index == -1){
                            data = "Invitation not exist\n"; 
                        }
                        else if(users[u_index].invitation[invite_index].invitation_code != code){
                            data = "Your invitation code is incorrect\n"; 
                        }
                        else if(rooms[r_index].status == 1){
                            data = "Game has started, you can't join now\n"; 
                        }
                        else{
                            data = "You join game room " + rooms[r_index].id + "\n"; 
                            users[u_index].room = rooms[r_index].id;
                            rooms[r_index].player.push_back(users[u_index].username);
                            for(int i = 0; i < users.size(); i++){
                                if(users[i].fd != sd && users[i].room == rooms[r_index].id){
                                    string welcome_data = "Welcome, " + users[u_index].username + " to game!\n";
                                    sendto(users[i].fd, welcome_data.c_str(), strlen(welcome_data.c_str()),0, (const struct sockaddr*)&address,sizeof(users[i].fd));
                                }
                            }
                        }
                        sendto(sd, data.c_str(), strlen(data.c_str()),0, (const struct sockaddr*)&address,sizeof(sd));
                    }
                    else if(!strcmp(arg[0],"leave")){
                        int u_index = checkName(client_socket[i].second);
                        int r_index = checkRoom(users[u_index].room);
                        if(client_socket[i].second == ""){
                            data = "You are not logged in\n";
                        }
                        else if(users[u_index].room == "none"){
                            data = "You did not join any game room\n";
                        }
                        else if(rooms[r_index].manager == users[u_index].username){
                            data = "You leave game room " + rooms[r_index].id + "\n";
                            users[u_index].room = "none";
                            for(int i = 0; i < users.size(); i++){ // in this room -> send message
                                if(users[i].fd != sd && users[i].room == rooms[r_index].id){
                                    string leave_data = "Game room manager leave game room " + rooms[r_index].id + ", you are forced to leave too\n";
                                    users[i].room = "none";
                                    sendto(users[i].fd, leave_data.c_str(), strlen(leave_data.c_str()), 0, (const struct sockaddr*)&address, sizeof(users[i].fd));
                                }
                            }
                            for(int i = 0; i < users.size(); i++){ // all users
                                for(int j = (users[i].invitation.size()-1); j >= 0; j--){ //all invitation
                                    if(users[i].invitation[j].inviter == users[u_index].username && users[i].invitation[j].room_id == rooms[r_index].id){
                                        users[i].invitation.erase(users[i].invitation.begin()+j);
                                    }
                                }
                            }
                            rooms.erase(rooms.begin()+r_index);
                        }
                        else if(rooms[r_index].manager != users[u_index].username && rooms[r_index].status == 1){
                            int p_index = findPlayerIndex(r_index, users[u_index].username);
                            rooms[r_index].status = 0;
                            rooms[r_index].player.erase(rooms[r_index].player.begin()+p_index);
                            rooms[r_index].current_player = 0;
                            data = "You leave game room " + rooms[r_index].id + ", game ends\n";
                            users[u_index].room = "none";
                            for(int i = 0; i < users.size(); i++){ // in this room -> send message
                                if(users[i].fd != sd && users[i].room == rooms[r_index].id){
                                    string leave_data = users[u_index].username + " leave game room " + rooms[r_index].id + ", game ends\n";
                                    sendto(users[i].fd, leave_data.c_str(), strlen(leave_data.c_str()), 0, (const struct sockaddr*)&address, sizeof(users[i].fd));
                                }
                            }
                        }
                        else if(rooms[r_index].manager != users[u_index].username && rooms[r_index].status == 0){
                            int p_index = findPlayerIndex(r_index, users[u_index].username);
                            rooms[r_index].player.erase(rooms[r_index].player.begin()+p_index);
                            data = "You leave game room " + rooms[r_index].id + "\n";
                            users[u_index].room = "none";
                            for(int i = 0; i < users.size(); i++){ // in this room -> send message
                                if(users[i].fd != sd && users[i].room == rooms[r_index].id){
                                    string leave_data = users[u_index].username + " leave game room " + rooms[r_index].id + "\n";
                                    sendto(users[i].fd, leave_data.c_str(), strlen(leave_data.c_str()), 0, (const struct sockaddr*)&address, sizeof(users[i].fd));
                                }
                            }
                        }
                        sendto(sd, data.c_str(), strlen(data.c_str()),0, (const struct sockaddr*)&address,sizeof(sd));
                    }
                    else if(!strcmp(arg[0],"start") && !strcmp(arg[1],"game")){
                        string rounds = arg[2];
                        int u_index = checkName(client_socket[i].second);
                        int r_index = checkRoom(users[u_index].room);
                        if(client_socket[i].second == ""){
                            data = "You are not logged in\n";
                        }
                        else if(users[u_index].room == "none"){
                            data = "You did not join any game room\n";
                        }
                        else{
                            if(rooms[r_index].manager != users[u_index].username){
                                data = "You are not game room manager, you can't start game\n";
                            }
                            else if(rooms[r_index].status == 1){
                                data = "Game has started, you can't start again\n";
                            }
                            else if(length == 4){ // with guess number
                                string guess = arg[3];
                                int guess_number;
                                try{
                                    guess_number = stoi(guess);
                                }
                                catch(exception &e){
                                    data = "Please enter 4 digit number with leading zero\n";
                                    sendto(sd, data.c_str(), strlen(data.c_str()),0, (const struct sockaddr*)&address,sizeof(sd));
                                    continue;
                                }
                                int flag = 0;
                                for(int i = 0; i < 4; i++){
                                    if(guess[i]<'0' || guess[i]>'9'){
                                        flag = 1;
                                        break;
                                    }
                                }
                                if(flag == 1 || guess.size() != 4){
                                    data = "Please enter 4 digit number with leading zero\n";
                                    sendto(sd, data.c_str(), strlen(data.c_str()),0, (const struct sockaddr*)&address,sizeof(sd));
                                    continue;
                                }
                                rooms[r_index].guess_number = guess;
                                rooms[r_index].rounds = stoi(rounds);
                                rooms[r_index].current_player = 0;
                                rooms[r_index].status = 1;                                
                                for(int i = 0; i < users.size(); i++){
                                    if(users[i].room == rooms[r_index].id){
                                        string start_data = "Game start! Current player is " + rooms[r_index].player[rooms[r_index].current_player] + "\n";
                                        sendto(users[i].fd, start_data.c_str(), strlen(start_data.c_str()),0, (const struct sockaddr*)&address,sizeof(users[i].fd));
                                    }
                                }
                            }
                            else if(length == 3){ // without guess number                                
                                rooms[r_index].guess_number = to_string(rand() % (9999 - 1000 + 1) + 1000);
                                rooms[r_index].rounds = stoi(rounds);
                                rooms[r_index].current_player = 0;
                                rooms[r_index].status = 1;                                
                                for(int i = 0; i < users.size(); i++){
                                    if(users[i].room == rooms[r_index].id){
                                        string start_data = "Game start! Current player is " + rooms[r_index].player[rooms[r_index].current_player] + "\n";
                                        sendto(users[i].fd, start_data.c_str(), strlen(start_data.c_str()),0, (const struct sockaddr*)&address,sizeof(users[i].fd));
                                    }
                                }
                            }
                        }
                        sendto(sd, data.c_str(), strlen(data.c_str()),0, (const struct sockaddr*)&address,sizeof(sd));
                    }
                    else if(!strcmp(arg[0],"guess")){
                        string guess_arg = arg[1];
                        int u_index = checkName(client_socket[i].second);
                        int r_index = checkRoom(users[u_index].room);
                        if(client_socket[i].second == ""){
                            data = "You are not logged in\n";
                        }
                        else if(users[u_index].room == "none"){
                            data = "You did not join any game room\n";
                        }
                        else if(rooms[r_index].status == 0 && rooms[r_index].manager == users[u_index].username){
                            data = "You are game room manager, please start game first\n";
                        }
                        else if(rooms[r_index].status == 0 && rooms[r_index].manager != users[u_index].username){
                            data = "Game has not started yet\n";
                        }
                        else if(rooms[r_index].status == 1 && rooms[r_index].player[rooms[r_index].current_player] != users[u_index].username){
                            data = "Please wait..., current player is " + rooms[r_index].player[rooms[r_index].current_player] + "\n";
                        }
                        else{
                            int guess_number;
                            try{
                                guess_number = stoi(guess_arg);
                            }
                            catch(exception &e){
                                data = "Please enter 4 digit number with leading zero\n";
                                sendto(sd, data.c_str(), strlen(data.c_str()),0, (const struct sockaddr*)&address,sizeof(sd));
                                continue;
                            }
                            int flag = 0;
                            for(int i = 0; i < 4; i++){
                                if(guess_arg[i] < '0' || guess_arg[i] > '9'){
                                    flag = 1;
                                    break;
                                }
                            }
                            if(flag == 1 || guess_arg.size() != 4){
                                data = "Please enter 4 digit number with leading zero\n";
                                sendto(sd, data.c_str(), strlen(data.c_str()),0, (const struct sockaddr*)&address,sizeof(sd));
                                continue;
                            }

                            string guess = guess_arg;
                            string ans = rooms[r_index].guess_number;
                            int numA = 0;
                            int numB = 0;
                            int visited[4];
                            for(int i = 0; i < 4; i++){
                                visited[i] = 0;
                            }
                            for(int i = 0; i < 4; i++){
                                if(guess[i] == ans[i]){
                                    numA++;
                                    visited[i] = 1;
                                }
                            }
                            for(int i = 0; i < 4; i++){
                                for(int j = 0; j < 4; j++){
                                    if(visited[i] == 1 || visited[j] == 1){
                                        continue;
                                    }
                                    if(guess[i] == ans[j] && i != j){
                                        numB++;
                                        break;
                                    }
                                }
                            }
                            if(numA == 4){
                                for(int i = 0; i < users.size(); i++){
                                    if(users[i].room == rooms[r_index].id){
                                        string bingo_data = users[u_index].username + " guess '" + guess_arg + "' and got Bingo!!! " + users[u_index].username + " wins the game, game ends\n";;
                                        sendto(users[i].fd, bingo_data.c_str(), strlen(bingo_data.c_str()),0, (const struct sockaddr*)&address,sizeof(users[i].fd));
                                    }
                                }
                                rooms[r_index].status = 0;
                                rooms[r_index].guess_number = "";
                                rooms[r_index].current_player = 0;
                                continue;
                            }
                            if(rooms[r_index].current_player == rooms[r_index].player.size() - 1){ // if he is the last player
                                rooms[r_index].rounds = rooms[r_index].rounds - 1;
                            }
                            if(rooms[r_index].rounds == 0){
                                for(int i = 0; i < users.size(); i++){
                                    if(users[i].room == rooms[r_index].id){
                                        string fail_data = users[u_index].username + " guess '" + guess_arg + "' and got '" + to_string(numA) + "A" + to_string(numB) + "B'\nGame ends, no one wins\n";
                                        sendto(users[i].fd, fail_data.c_str(), strlen(fail_data.c_str()),0, (const struct sockaddr*)&address,sizeof(users[i].fd));
                                    }
                                }
                                rooms[r_index].status = 0;
                                rooms[r_index].guess_number = "";
                                rooms[r_index].current_player = 0;
                                continue;
                            }
                            else if(rooms[r_index].rounds != 0){
                                for(int i = 0; i < users.size(); i++){
                                    if(users[i].room == rooms[r_index].id){
                                        string guess_data = users[u_index].username + " guess '" + guess_arg + "' and got '" + to_string(numA) + "A" + to_string(numB) + "B'\n";
                                        sendto(users[i].fd, guess_data.c_str(), strlen(guess_data.c_str()),0, (const struct sockaddr*)&address,sizeof(users[i].fd));
                                    }
                                }
                                rooms[r_index].current_player = (rooms[r_index].current_player + 1) % rooms[r_index].player.size();
                                continue;
                            }
                        }
                        sendto(sd, data.c_str(), strlen(data.c_str()),0, (const struct sockaddr*)&address,sizeof(sd));
                    }
                }
	        }
        }
    }
	return 0;
}
