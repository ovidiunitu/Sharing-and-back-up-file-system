
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <cstring>
#include <string>
#include <iostream>
#include <sstream>
#include <fstream>
#include <vector>
#include <map>
#include <set>
#include <errno.h>

#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <ostream>
#include <fcntl.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "errors_ok.h"
using namespace std;


fstream f, g;


struct sockaddr_in serv_addr;
int sockfd, portno;
fd_set read_fds;	//multimea de citire folosita in select()
fd_set tmp_fds;	//multime folosita temporar
int fdmax;		//valoare maxima file descriptor din multimea read_fds




string afiseaza_eroare (int cod_eroare)
{

	switch (cod_eroare) {
	case -1: return  string("Clientul nu e autentificat");
	case -2: return  string("Sesiune deja deschisa");
	case -3: return  string("User/parola gresita");
	case -4: return  string("Fisier inexistent");
	case -5: return  string("Descarcare interzisa");
	case -6: return  string("Fisier deja partajat");
	case -7: return  string("Fisier deja privat");
	case -8: return  string("Brute-force detectat");
	case -9: return  string("Fiser existent pe server");
	case -10: return  string("Fisier in transfer");
	case -11: return string("Utilizator inexistent");
	case -12: return  string("Comanda invalida");
	case USER_DEJA_LOGAT: return string("Userul este deja logat");
	case LOGOUT_SUCCES: return string("Logout cu succes");
	default : return string("nu ar trebui sa afisezi!!");

	}
	return "";
}

string afiseaza_ok (int ok_code)
{

	switch (ok_code) {
	case 1: return  string("Autentificare cu SUCCES");


	}
	return "";
}





struct fisier
{
	string name;
	char nume[BUFLEN];
	long long size_in_bytes;
	bool shared;
	fisier() { shared = false; size_in_bytes = -1;};
	fisier(char p[], long long size, char ch)
	{
		if (ch == 'S')
			shared = true;
		else
			shared = false;
		strcpy(nume, p);
		name =  string(p);
		size_in_bytes = size;
	}

};
struct utilizator
{
	string name;
	string pass;
	char nume[LENGHT];
	char parola[LENGHT];
	vector<fisier> files;
	utilizator() {}
	utilizator(char user_name[], char user_pass[])
	{
		strcpy(nume, user_name);
		strcpy(parola, user_pass);
		name = string(user_name);
		pass = string(user_pass);

	}
};

vector <utilizator> Utilizatori;
map < string, int> user_map; // am indicie in vectorul Utilizator

map < int, string > socket_user_map; // map de socketuri-> utilizatorul string e conectat
map < string, vector<int> > user_socket_map; // map de socketuri-> utilizatorul string e conectat
map <int, int> socket_online; // socket, number of logins

map <int, string> socket_file; // socketul fisierului si numele fisierului
map <int, int > socktcp_file;  // socketul care vine de la client si socketl unde scriu informatia
map <int, long long > socktcp_size; // socket si dimensiunea fisierului

vector <string> files_in_transfer;
vector <string> files_in_download;
set <int> clienti;


// citesc lista de utlizatori
void readUsers()
{
	int n, i;
	char user_name[LENGHT], user_pass[LENGHT];
	f >> n;
	for (i = 1; i <= n; i++)
	{
		f >> user_name >> user_pass; // nume si parola
		// adaug
		Utilizatori.push_back( utilizator(user_name, user_pass));

		user_map[Utilizatori[i - 1].name] = i - 1; // setez indicele unui utilizator in cadrul vectorului

		int result = mkdir(user_name, ACCESSPERMS); // fac folder daca nu are
		if (result == 0)
		{
			cout << "Am creat folder pentru userul: " << user_name << endl;
		}
	}
}

// functie ce verifica daca userul are fisierul file
long long user_has_file(int user_index, char *file)
{
	// ma uit in folderul lui
	string path = Utilizatori[user_index].name + '/' + string(file);
	struct stat st;
	if (stat(path.c_str(), &st) != 0)
	{
		return -1;
	}
	return st.st_size;
}
// daca exista fisierul respectiv
long long exists_file(string path)
{
	struct stat st;
	if (stat(path.c_str(), &st) != 0)
	{
		return -1;
	}
	return st.st_size;
}

void add_shared_file_to_user(int userIdex, char *file )
{
	long long size_of_file = user_has_file(userIdex, file);

	if (size_of_file < 0)
	{
		cout << "Fisierul " << file << " al userului " << Utilizatori[userIdex].name << " nu exista" << endl << flush;
		return ;
	}

	Utilizatori[userIdex].files.push_back(fisier(file, size_of_file, 'S'));
}

// citesc fisierele partajate
void readSharedFiles()
{
	int n;
	char lin[BUFLEN];
	char user_name[LENGHT];
	g >> n;
	g.get();// sar peste enter
	while (n--)
	{
		g.getline(lin, BUFLEN - 1);
		char *p =  strtok(lin, ":");// fac split dupa :
		strcpy(user_name, p);
		p = strtok(NULL, ":");
		// daca nu exista in sistem
		if (user_map.find(string(user_name)) == user_map.end())
		{
			cout << "Utilizatorul " << user_name << " nu exista in sistem!" << endl;
			continue;
		}
		// adaug fisierul
		add_shared_file_to_user(user_map[string(user_name)], p);
	}
}
void error(const char *msg)
{
	perror(msg);
	exit(1);
}

// s-a conectat un nou client
void new_conection() {

	struct sockaddr_in cli_addr;
	int client_len = sizeof(cli_addr);
	int newsockfd;
	// socket nou
	newsockfd = accept(sockfd, (struct  sockaddr*)&cli_addr, (socklen_t*)&client_len);
	if (newsockfd == -1)
	{
		error("ERROR in accept");
		return ;
	}
	FD_SET(newsockfd, &read_fds);
	if (newsockfd > fdmax)
		fdmax = newsockfd;
	// nu s-a autentificat niciun utilizator
	socket_online[newsockfd] = 0;
	socket_user_map[newsockfd] = "";
	clienti.insert(newsockfd);// inserez clientul nou
	cout << "Noua conexiune de la " << newsockfd << '\n';

}
// trimit mesaj cate utilizator
void send_message_to_user(int user_socket, char message[], int len)
{
	int sended;
	sended = send(user_socket, message, len, 0);
	if (sended < 0)
	{
		cout << "nu am putut trimite la" << user_socket << " \n";
	}
}
// trimit eroare catre utilizator
void send_error_to_user(int user_socket, int error_code)
{
	char buffer[BUFLEN];
	buffer[0] = _ERROR; // it's an error
	memcpy(buffer + 1, &error_code, sizeof(int));
	send_message_to_user(user_socket, buffer, 1 + sizeof(int));
}
// trimit mesaj de succes catre utilizator
void send_OK_to_user(int user_socket, int OK_code, char command[])
{
	char buffer[BUFLEN];
	buffer[0] = _SUCCES;
	memcpy(buffer + 1, &OK_code, sizeof(int) );// copiez codul de succes
	int message_lenght = sizeof(int) + 1;

	if (OK_code == AUTENTIFICARE_SUCCES)
	{
		stringstream sin(command);
		char temp[BUFLEN];
		sin >> temp;
		sin >> temp; // aici avem numele userului
		// trimit numele userului
		memcpy(buffer + message_lenght, temp, strlen(temp) + 1);
		message_lenght += strlen(temp) + 1;

	}

	if (OK_code == TRIMITE_STRING)
	{
		// string pur si simplu
		memcpy(buffer + message_lenght, command, strlen(command) + 1);
		message_lenght += strlen(command) + 1;
	}
	if (OK_code == ACCESS_UPLOAD)
	{
		// copiem si informatiile aditioanle port etc..
		memcpy(buffer + message_lenght, command, sizeof(int) + strlen(command + sizeof(int)) + 1);
		message_lenght += sizeof(int) + strlen(command + sizeof(int)) + 1;
	}
	if (OK_code ==  ACCESS_DOWNLOAD)
	{
		memcpy(buffer + message_lenght, command, strlen(command) + 1);
		message_lenght += strlen(command) + 1;
	}
	if (OK_code == FISIER_PARTAJAT || OK_code == FISIER_PRIVAT || OK_code == FISIER_STERS)
	{
		// numele fisierului
		memcpy(buffer + message_lenght, command, strlen(command) + 1);
		message_lenght += strlen(command) + 1;
	}

	// trimitem mesajul
	send_message_to_user(user_socket, buffer, message_lenght);
}
// verific daca s-a autentificat userul
int authenticate_user(char user[], char pass[])
{
	string user_name(user);
	string password(pass);
	// userul nu exista 
	if (user_map.find(user_name) == user_map.end())
		return 0;
	// parola nu e buna
	int user_index = user_map[user_name];
	if (Utilizatori[user_index].pass != pass)
		return 0;
	return 1;
}
// s-a deconectat un socket
void remove_socket_userSockMap(int sock)
{

	if (socket_user_map.find(sock) ==  socket_user_map.end())
		return ;
	// numele utilizatorului conectat
	string nume = socket_user_map[sock];
	if (nume != "")
	{
		vector<int>& v = user_socket_map[nume];
		if (v.size() == 0)
			user_socket_map.erase(nume);
		else
		{
			// sterg socketul din lista de socketuri
			int  i, indice = -1;
			for (i = 0; i < (int)v.size(); i++)
				if (v[i] == sock)
				{
					indice = i;
					break;
				}
			// l-am gasit
			if (indice != -1)
			{
				((vector<int>)(user_socket_map[nume])).erase( ((vector<int>)(user_socket_map[nume])).begin() + indice);
				if (user_socket_map[nume].size() == 0)
					user_socket_map.erase(nume);

			}
			else
			{
				cout << "something is wrong in remove_socket_userSockMap" << endl << flush;
			}
		}

	}
}

// inchid conexiunea cu clientul
void close_client_socket(int user_socket)
{
	close(user_socket);
	// scot din mapul de useri
	socket_user_map.erase(user_socket);
	remove_socket_userSockMap(user_socket);
	// si sterg logarile respective
	socket_online.erase(user_socket);
	clienti.erase(user_socket);
	// si din fisierul pe care ascult
	FD_CLR(user_socket, &read_fds);
}
// caut daca un user este in sistem
int is_user_in_system(char user[])
{
	string user_name(user);

	int i;
	for (i = 0; i < (int)Utilizatori.size(); i++)
		if (Utilizatori[i].name == user_name )// coincide numele
			return 1;

	return 0;
}

int is_user_online(char user[])
{
	if (user_socket_map.find(string(user)) == user_socket_map.end())
		return 0;
	return 1;

}
// daca un socket este conectat
int is_socket_online (int sock)
{
	if (socket_online.find(sock) == socket_online.end())
		return 0;
	return 1;
}
// daca un socket este logat
int is_socket_loged(int sock)
{
	if ( is_socket_online(sock) == 0)
		return 0;
	string nume =  socket_user_map[sock];
	if (nume == "")
		return 0;
	return 1;
}
// autentific un user
void login_user(int user_socket, char command[], int len)
{

	stringstream sin(command);
	char *p  = strtok(command, " \t"); 
	// parsez login , username si parola
	char *user;
	if ( p ==  NULL ) {
		send_error_to_user(user_socket, COMANDA_INVALIDA);
		return ;
	}
	user  = strtok(NULL, " \t");
	if (user == NULL) {
		send_error_to_user(user_socket, COMANDA_INVALIDA);
		return ;
	}

	char *pass;
	pass  = strtok(NULL, " \t");
	if (pass == NULL) {
		send_error_to_user(user_socket, COMANDA_INVALIDA);
		return ;
	}
	// daca a reusit autentificarea
	if (authenticate_user(user, pass))
	{
		// salvez toate informatiile despre acesta
		socket_online[user_socket] = 0;
		socket_user_map[user_socket] =  string(user);
		user_socket_map[string(user)].push_back ( user_socket);
		send_OK_to_user(user_socket, AUTENTIFICARE_SUCCES, user);


	}
	else
	{
		// maresc numarul de logari
		socket_online[user_socket]++;
		// am detectat bruteforce
		if (socket_online[user_socket] == BRUTE_FORCE_ATTEMPS)
		{
			send_error_to_user(user_socket, BRUTE_FORCE);
			close_client_socket(user_socket);
			return ;
		}
		send_error_to_user(user_socket, USER_PASS_GRESIT);
	}
}

// analiza comenzilor din puncte de vedere al corectitudinii 
int valid_command (string command)
{
	if (command == "login")
		return 1;

	if (command == "logout")
		return 1;
	if (command == "getuserlist")
		return 1;
	if (command == "getfilelist")
		return 1;
	if (command == "download")
		return 1;
	if (command == "upload")
		return 1;
	if (command == "share")
		return 1;
	if (command == "unshare")
		return 1;
	if (command == "delete")
		return 1;
	if (command == "quit")
		return 1;
	if (command == "upl")
		return 1;
	return 0;
}
// deloghez userul conectat
void logout_user(int user_socket)
{
	string nume = socket_user_map[user_socket];
	socket_user_map.erase(user_socket);
	remove_socket_userSockMap(user_socket);
}

void get_user_list(int user_socket)
{
	string str;
	for (int i = 0 ; i < (int)Utilizatori.size(); i++)
		str = str + Utilizatori[i].name + '\n';
	char information[BUFLEN];
	strcpy(information, str.c_str());
	// daca am prea multa informatie trimit pe rand
	while (FOREVER)
	{
		if (str.size() > BUFLEN - 2)
		{
			strncpy(information, str.c_str(), BUFLEN - 2);
			str.erase(0, BUFLEN - 2);
			send_OK_to_user(user_socket, TRIMITE_STRING, information);
		}
		else
		{
			strcpy(information, str.c_str());
			send_OK_to_user(user_socket, TRIMITE_STRING, information);
			break;
		}
	}


}
// aflu dimensiunea unui fisier
string get_file_size(char path[])
{
	struct stat st;
	if (stat(path, &st) != 0)
	{
		return "-1";
	}
	char numar[LENGHT];
	sprintf(numar, "%lld", (long long)st.st_size);
	return string(numar);

}
// verific daca un fisier este partajat
string is_shared(int user_index, char file[])
{
	utilizator x = Utilizatori[user_index];

	for (vector<fisier>::iterator it = x.files.begin(); it != x.files.end(); ++it)
	{
		if (strcmp(it->nume, file) == 0)
		{
			if (it->shared == true)
				return "SHARED";
			else
				return "eroare??"; // nu ar trebui sa intre aici
		}
	}

	return "PRIVATE";

}
// verific daca un anumit fisier este partajat de un anumit user
int is_file_shared_by_user(string user, string file)
{
	int indx = user_map[user];
	char temp[BUFLEN];
	strcpy(temp, file.c_str());
	string result = is_shared(indx, temp);
	if (result == "SHARED")
		return 1;
	else
		return 0;
}
void get_file_list(int user_socket, char command[], int len)
{
	// parsez getfilelist si username
	char *p =  strtok(command, " \t");
	if (p == NULL)
	{
		send_error_to_user(user_socket, COMANDA_INVALIDA);
		return;
	}
	char *user_name = strtok(NULL, "\t ");

	if (user_name == NULL)
	{
		send_error_to_user(user_socket, COMANDA_INVALIDA);
		return;
	}
	if (is_user_in_system(user_name) == 0 )
	{
		send_error_to_user(user_socket, UTILIZATOR_INEXISTENT);
		return;
	}
	// parcurg direcotorul utilizatorului curent
	string str;
	DIR *dir;
	struct dirent *dp;
	char *file;
	dir = opendir(user_name);
	char path[BUFLEN], path_to_file[BUFLEN];
	strcpy(path, user_name);
	strcat(path, "/");
	int user_index =  user_map[string(user_name)];
	while ((dp = readdir(dir)) != NULL)
	{
		file = dp->d_name;
		// sar peste . si ..
		if ( strcmp(dp->d_name, ".") && strcmp(dp->d_name, "..") )
		{
			strcpy(path_to_file, path);
			file = dp->d_name;
			strcat(path_to_file, file);
			// adaug informatia optinuta
			str += string(file) + "\t\t" + get_file_size(path_to_file) + " bytes\t\t\t" + is_shared(user_index, file) + '\n';
		}
	}

	char information[BUFLEN];
	// trimit pe bucat informatia	
	while (FOREVER)
	{
		if (str.size() > BUFLEN - 2)
		{
			strncpy(information, str.c_str(), BUFLEN - 2);
			str.erase(0, BUFLEN - 2);
			send_OK_to_user(user_socket, TRIMITE_STRING, information);
		}
		else
		{
			strcpy(information, str.c_str());
			send_OK_to_user(user_socket, TRIMITE_STRING, information);
			break;
		}
	}

}
// verific daca un fisier este in transfer (uplaod)
int in_transfer(string s)
{
	vector<string>::iterator it;
	for (int i = 0 ; i < (int)files_in_transfer.size(); i++)
		if (files_in_transfer[i] == s)
			return 1;
	return 0;
}
//verific daca un fisier se downloadeaza
int in_download_transferr(string s)
{
	vector<string>::iterator it;
	for (int i = 0 ; i < (int)files_in_download.size(); i++)
		if (files_in_download[i] == s)
			return 1;
	return 0;
}

// userul uploadeaza un fisier
void upload(int user_socket, char command[], int len)
{
	stringstream sin(command);
	char file_name[BUFLEN];


	string file;
	sin >> file; // upload
	// elimin spatiile albe
	getline(sin, file);
	while (file.size() > 0 && ((file.at(0) == ' ') || (file.at(0) == '\t')) )
		file.erase(file.begin());


	if (file == "")
	{
		send_error_to_user(user_socket, COMANDA_INVALIDA);
		return ;
	}

	strcpy(file_name, file.c_str());

	string user_name = socket_user_map[user_socket];
	int userIdx = user_map[user_name];
	string path_to_file;
	path_to_file = user_name + "/" + file_name;
	// determin daca fisierul de uploadeaza deja
	if (in_transfer(path_to_file))
	{
		char temp[BUFLEN] = "Fisierul se uploadeaza deja";

		send_OK_to_user(user_socket, TRIMITE_STRING, temp);

		return;
	}
	// daca mai exista un astfel de fisier pe server
	if (user_has_file(userIdx, file_name) > 0)
	{
		send_error_to_user(user_socket, FISIER_EXISTENT_PE_SERVER);
		return;
	}
	// adaug in lista de transferuri
	files_in_transfer.push_back(path_to_file);
	char buffer[BUFLEN] = {0};
	// deschid fisierul
	int sursa = open(path_to_file.c_str(), O_RDWR | O_CREAT, 0777);
	memcpy(buffer, &sursa, sizeof(int));
	memcpy(buffer + sizeof(int), file_name, strlen(file_name) + 1);
	socket_file[sursa] = path_to_file;

	send_OK_to_user(user_socket, ACCESS_UPLOAD, buffer);

}
// userul vrea sa descarce 
void download(int user_socket, char command[], int len)
{
	// parsez comanda
	stringstream sin(command);
	string instruction, user, file;
	sin >> instruction;
	sin >> user;
	if (user == "")
	{
		send_error_to_user(user_socket, COMANDA_INVALIDA);
		return ;
	}

	if (user == "@")
		user = socket_user_map[user_socket];

	char nume_user[BUFLEN];
	strcpy(nume_user, user.c_str());
	// daca userul este in sistem
	if (is_user_in_system(nume_user) == 0 )
	{
		send_error_to_user(user_socket, UTILIZATOR_INEXISTENT);
		return;
	}
	getline(sin, file);
	// elimin spatiile albe de la inceput
	while (file.size() > 0 && ((file.at(0) == ' ') || (file.at(0) == '\t')) )
		file.erase(file.begin());
	if (file == "")
	{
		send_error_to_user(user_socket, COMANDA_INVALIDA);
		return ;
	}


	cout << "Utilizatorul " << user << " vrea sa descarce " << file << endl << flush;

	string path_to_file;
	path_to_file = user + "/" + file;
	// determin daca este in transfer
	if (in_transfer(path_to_file))
	{
		send_error_to_user(user_socket, FISIER_IN_TRANSFER);
		return;
	}


	if  (exists_file(path_to_file) == -1)
	{
		send_error_to_user(user_socket, FISIER_NEEXISTENT);
		return;
	}
	if (user ==  socket_user_map[user_socket] || is_file_shared_by_user(user, file))
	{
		char buffer[BUFLEN - 6] = {0};

		// deschid fisierul
		int read_file = open( path_to_file.c_str(), O_RDONLY);
		if (read_file < 0)
		{
			cout << "Nu am putut sa deschid fisierul\n" << endl << flush;
			perror("Erroare in download");
		}
		long long dimension = exists_file(path_to_file);
		socket_file[read_file] = path_to_file;
		// fisierul se descarca
		files_in_download.push_back(path_to_file);
		sprintf(buffer, "%d %lld %s\n", read_file, dimension, file.c_str());
		send_OK_to_user(user_socket, ACCESS_DOWNLOAD, buffer);
		return;

	}
	else
	{
		send_error_to_user(user_socket, DESCARCARE_INTERZISA);
	}

}
// fac un fisier sa fie partajat
void make_share (int user_socket, char command[], int len)
{

	stringstream sin(command);
	string instruction, file;
	sin >> instruction;

	getline(sin, file);
	// sterg caracterele albe
	while ( ((file.at(0) == ' ') || (file.at(0) == '\t')) && file.size() > 0)
		file.erase(file.begin());

	if (file == "")
	{
		send_error_to_user(user_socket, COMANDA_INVALIDA);
		return ;
	}
	// determin numele si indicele in vector
	string nume = socket_user_map[user_socket];
	int idx = user_map[nume];

	for ( vector<fisier>::iterator i = Utilizatori[idx].files.begin(); i != Utilizatori[idx].files.end(); ++i)
	{
		if (i->name == file) {
			send_error_to_user(user_socket, FISIER_DEJA_PARTAJAT);
			return;
		}

	}


	long long size_of_file = user_has_file(idx, (char *)file.c_str());

	if (size_of_file < 0)
	{
		send_error_to_user(user_socket, FISIER_NEEXISTENT);
		return ;
	}

	add_shared_file_to_user(idx, (char *)file.c_str());

	char temp[BUFLEN];

	sprintf(temp, "Fisierul %s  a fost partajat.\n", file.c_str());
	send_OK_to_user(user_socket, FISIER_PARTAJAT, temp);



}

void make_unshare (int user_socket, char command[], int len)
{

	stringstream sin(command);
	string instruction, file;
	sin >> instruction;

	getline(sin, file);
	// steg caracterele albe de la inceput
	while ( ((file.at(0) == ' ') || (file.at(0) == '\t')) && file.size() > 0)
		file.erase(file.begin());

	if (file == "")
	{
		send_error_to_user(user_socket, COMANDA_INVALIDA);
		return ;
	}
	// determin numele si indicele in vector
	string nume = socket_user_map[user_socket];
	int idx = user_map[nume];

	long long size_of_file = user_has_file(idx, (char *)file.c_str());

	if (size_of_file < 0)
	{
		send_error_to_user(user_socket, FISIER_NEEXISTENT);
		return ;
	}


	// verifica daca fisierul este partajat
	for ( vector<fisier>::iterator i = Utilizatori[idx].files.begin(); i != Utilizatori[idx].files.end(); ++i)
	{
		if (i->name == file) {

			Utilizatori[idx].files.erase(i); // vezi daca merge !!!
			char temp[BUFLEN];
			sprintf(temp, "Fisierul %s  a fost setat ca private.\n", file.c_str());

			cout << temp << endl << flush;
			send_OK_to_user(user_socket, FISIER_PRIVAT, temp);
			return;
		}

	}


	send_error_to_user(user_socket, FISIER_DEJA_PRIVAT);

}



// sterfisieul
void delete_file(int user_socket, char command[], int len)
{
	stringstream sin(command);
	string instruction, file;
	sin >> instruction;

	getline(sin, file);
	// elimin caracterele albe
	while (file.size() > 0 && ((file.at(0) == ' ') || (file.at(0) == '\t')) )
	{
		file.erase(file.begin());
	}
	if (file == "")
	{
		send_error_to_user(user_socket, COMANDA_INVALIDA);
		return ;
	}
	
	// determin numele si indicele in vector
	string nume = socket_user_map[user_socket];
	int idx = user_map[nume];

	long long size_of_file = user_has_file(idx, (char *)file.c_str());

	if (size_of_file < 0)
	{
		send_error_to_user(user_socket, FISIER_NEEXISTENT);
		return ;
	}

	string path = nume + '/' + file;
	// verofic sa nu fie in transfer
	if ( in_transfer(path) || in_download_transferr(path))
	{
		send_error_to_user(user_socket, FISIER_IN_TRANSFER);
		return ;
	}
	// il sterg
	unlink(path.c_str());
	char temp[BUFLEN];
	// trimit mesaj la client
	sprintf(temp, "Fisierul %s a fost sters", file.c_str());
	for ( vector<fisier>::iterator i = Utilizatori[idx].files.begin(); i != Utilizatori[idx].files.end(); ++i)
	{
		if (i->name == file) {

			Utilizatori[idx].files.erase(i); // vezi daca merge !!!
			send_OK_to_user(user_socket, FISIER_STERS, temp);
			return;
		}

	}
	send_OK_to_user(user_socket, FISIER_STERS, temp);
}

// un client vrea sa se deconecteze
void quit(int user_socket)
{
	char temp[BUFLEN] = "out";
	send_OK_to_user(user_socket, IESIRE_SISTEM, temp);
	clienti.erase(user_socket);
	FD_CLR(user_socket, &read_fds);
	close(user_socket);
}
// aleg o actiune in functie de instructiunea primita
void do_action(string instruction, int user_socket,  char command[], int len)
{
	if (instruction == "login")
	{
		login_user(user_socket, command, len);
		return ;
	}
	if (instruction == "logout")
	{
		logout_user(user_socket);
	}

	if (instruction == "getuserlist")
	{
		get_user_list(user_socket);
	}
	if (instruction == "getfilelist")
	{
		get_file_list(user_socket, command, len);
	}

	if (instruction == "upload")
	{
		upload(user_socket, command, len);
	}
	if (instruction == "download")
	{
		download(user_socket, command, len);
	}
	if (instruction == "share")
	{
		make_share(user_socket, command, len);
	}

	if (instruction == "unshare")
	{
		make_unshare(user_socket, command, len);
	}

	if (instruction == "delete")
	{
		delete_file(user_socket, command, len);
	}
	if (instruction == "quit")
	{
		quit(user_socket);
	}


}
// analizez comanda primita de la client
void analyse_command(int user_socket, char command[], int len)
{
	string temp(command);
	stringstream sin(temp);
	char instruction[BUFLEN];
	sin >> instruction;
	string instructiune = string(instruction);
	if (valid_command(instructiune) == 0)// daca instructiunea este valida
	{
		send_error_to_user(user_socket, COMANDA_INVALIDA);
		return ;
	}

	do_action(instructiune, user_socket, command, len);
}
// creez o conexiune noua de upload
void set_upload_connection(int user_socket, char command[], int len)
{
	stringstream sin(command);
	string instruction;
	sin >> instruction;
	int file_descriptor;
	if (instruction == "u")
	{
		long long dimension;
		sin >> file_descriptor >> dimension;// fd si dimensiune
		socktcp_file[user_socket] = file_descriptor;
		socktcp_size[user_socket] = dimension;
		char buffer[BUFLEN] = "ack"; // ii spun ca am realizat conexiunea
		send(user_socket, buffer, strlen(buffer) + 1, 0);
	}

}
// sterg un fisier din lista de fisere ce se transfera
void remove_from_files_in_transfer(string s)
{
	int i;
	for (i = 0 ; i < (int)files_in_transfer.size(); i++ )
	{
		if (files_in_transfer[i] == s)
		{
			break;
		}
	}
	if (i < (int)files_in_transfer.size())
		files_in_transfer.erase(files_in_transfer.begin() + i);
}
// sterg un fisier din lista de fisiere ce se descarca
void remove_from_files_in_download(string s)
{
	int i;
	for (i = 0 ; i < (int)files_in_download.size(); i++ )
	{
		if (files_in_download[i] == s)
		{
			break;
		}
	}
	if (i < (int)files_in_download.size())
		files_in_download.erase(files_in_download.begin() + i);
}

// uploadez (scriu) informatie
void uploading(int user_socket, char buffer[], int len)
{
	long long ramas_de_upload = socktcp_size[user_socket];
	if (ramas_de_upload < len)
		len = ramas_de_upload;
	int write_fd = socktcp_file[user_socket];
	int scris = write(write_fd, buffer, len);// scriu
	socktcp_size[user_socket] -= scris;
	if (scris < 0)
		error("NU am reusit sa scriu in fisier!!");

	// nu mai am ce sa scriu
	if (socktcp_size[user_socket] == 0)
	{

		FD_CLR(user_socket, &read_fds);
		close(user_socket);
		close(write_fd);
		remove_from_files_in_transfer(socket_file[write_fd]); // socketul fisierului si numele fisierului
		// conxiunea se inchide si sterg informatiile despre aceasta
		socket_file.erase(write_fd);
		socktcp_file.erase(user_socket);
		socktcp_size.erase(write_fd);
		return ;
	}
	// confirm realizarea scrierii
	char temp[BUFLEN] = "ack";
	send(user_socket, temp, strlen(buffer) + 1, 0);


}
// downloadez (citesc) informatie
void downloading(int user_socket)
{
	int fd_read = socktcp_file[user_socket];


	char buffer[BUFLEN];
	buffer[0] = FROM_STATION;// e informatie de la server
	buffer[1] = DOWNLOAD_MESSAGE; // de tip download
	int copiat = read(fd_read, buffer + 2, BUFLEN - 2);
	socktcp_size[user_socket] -= copiat;
	if (socktcp_size[user_socket] == 0)// s-a terminat fisierul
	{
		int sended;
		if (copiat > 0)
			sended = send(user_socket, buffer, copiat + 2, 0);
		if (sended < 0)
			error("ERROR writing to socket");

		cout << "Am terminat upload\n\n" << endl << flush;
		FD_CLR(user_socket, &read_fds);
		close(user_socket);
		// sterg informatiile care nu mai conteaza
		socktcp_size.erase(user_socket);
		socktcp_file.erase(user_socket);


		remove_from_files_in_download(socket_file[fd_read]);
		socket_file.erase(fd_read);
		return;
	}

	int	sended = send(user_socket, buffer, copiat + 2, 0);
	if (sended < 0)
		error("ERROR writing to socket");

}
// setez conexiunea de download
void set_download_connection(int user_socket, char command[], int len)
{
	stringstream sin(command);
	string instruction;
	sin >> instruction;
	int file_descriptor;
	if (instruction == "d")
	{
		int dimension;
		sin >> file_descriptor >> dimension;
		// salvez informatile 
		socktcp_file[user_socket] = file_descriptor;
		socktcp_size[user_socket] = dimension;
		// trimit primul pachet
		downloading(user_socket);
	}



}
// functie ce analizeaza daca este comanda de tip download
// sau upload
void down_up_command(int user_socket, char command[], int len)
{

	if (command[0] == 'o')
	{
		downloading(user_socket);
	}

	if (command[0] == 'u')
	{
		set_upload_connection(user_socket, command, len);
	}

	if (command[0] == 'd')
	{
		set_download_connection(user_socket, command, len);
	}
	if (command[0] == UPLOAD_MESSAGE)
	{

		uploading(user_socket, command + 1, len - 1);
	}

}
// primesc comanda 
void receive_command(int user_socket)
{

	char buffer[BUFLEN];
	memset(buffer, 0, BUFLEN);
	int received;
	received = recv(user_socket, buffer, BUFLEN, 0);

	if (received < 0)
	{
		error("Error in receive");

	}
	if (received == 0) // s-a inchis o conexiune
	{
		close_client_socket(user_socket);
		return;
	}
	if (buffer[0] == FROM_USER) // e de la utilizator
	{
		clienti.insert(user_socket);
		analyse_command(user_socket, buffer + 1, received - 1);
	}
	else
	{
		clienti.erase(user_socket);
		down_up_command(user_socket, buffer + 1, received - 1);
	}

}
// severul vrea sa se inchida
void quit_server()
{
	char temp[BUFLEN] = "quit";
	// pargurg toti clientii conectati
	for (std::set<int>::iterator i = clienti.begin(); i != clienti.end(); ++i)
	{

		int user_socket = *i;
		send_OK_to_user(user_socket, IESIRE_SISTEM_SERVER, temp);
	}

	FD_CLR(0, &read_fds);
	FD_CLR(sockfd, &read_fds);
	close(sockfd);
	FD_CLR(0, &tmp_fds);

	cout << "Serverul o sa se inchida si nu va mai accepta comenzi sau conexiuni noi" << endl << flush;
	cout << "Asteptati eventualele fisiere ce se alfa in transfer" << endl << flush;

	g << "Serverul o sa se inchida si nu va mai accepta comenzi sau conexiuni noi" << endl << flush;
	g << "Asteptati eventualele fisiere ce se alfa in transfer" << endl << flush;
}
int main(int argc, char *argv[])
{

	if (argc < 4)
	{
		cout << "Serverul nu este apelata cu suficienti parametrii\n";
		exit(1);
	}

	f.open(argv[2], ios::in);
	g.open(argv[3], ios::in);

	readUsers();
	readSharedFiles();


	int i;

	//golim multimea de descriptori de citire (read_fds) si multimea tmp_fds
	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0)
		error("ERROR opening socket");

	portno = atoi(argv[1]);

	memset((char *) &serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;	// foloseste adresa IP a masinii
	serv_addr.sin_port = htons(portno);

	if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr)) < 0)
		error("ERROR on binding");

	listen(sockfd, MAX_CLIENTS);

	//adaugam noul file descriptor (socketul pe care se asculta conexiuni) in multimea read_fds
	FD_SET(sockfd, &read_fds);
	FD_SET(0, &read_fds);

	fdmax = sockfd;

	char buffer[BUFLEN];

	// main loop
	while (1) {
		tmp_fds = read_fds;
		if  (select(fdmax + 1, &tmp_fds, NULL, NULL, NULL) == -1)
			error("ERROR in select");

		if (FD_ISSET(0, &tmp_fds)) {
			cin.getline(buffer , BUFLEN);
			if (strcmp(buffer, "quit") == 0)
			{
				quit_server();
			}
			else
				cout << "Comanda invalida" << endl << flush;

		}
		for (i = 1; i <= fdmax; i++)
		{
			if (FD_ISSET(i, &tmp_fds)) // daca am conexiune
			{

				// a venit ceva pe socketul inactiv(cel cu listen) = o noua conexiune
				// actiunea serverului: accept()
				//cout << i << "\n";
				if ( i == sockfd)
					new_conection();
				else
					receive_command(i);

			}
		}
		// verific daca mai am de unde primii informatie
		int number_sockets = 0;
		for (i = 0; i <= fdmax; i++)
			if (i != sockfd && FD_ISSET(i, &read_fds))
			{
				number_sockets ++;
			}

		if (number_sockets == 0)
		{
			return 0;
		}
	}


	close(sockfd);

	return 0;
}


