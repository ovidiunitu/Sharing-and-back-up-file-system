
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
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <sys/types.h>
#include <ostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>


#include "errors_ok.h"


using namespace std;


fstream g;
int sockfd;
struct sockaddr_in serv_addr;
fd_set read_fds;	//multimea de citire folosita in select()
fd_set tmp_fds;		//multime folosita temporar
int fdmax;		//valoare maxima file descriptor din multimea read_fds
bool logged_in = false;
string current_user = "";
bool closed_conection = false;
int pid = getpid();


map<int, int > socketcp_file_map;
map<int, int > socketcp_file_size_map;




struct upload_file
{
	bool used;
	string name;
	int socket_on_server;
	int socket_file_local;
	int socket_tcp;
	long long  dimension;
	upload_file()
	{
		used = false;
		dimension = 0;
	}
	upload_file(char nume[], int serv_sock, int sock_file, int sock_tcp)
	{
		name = string(nume);
		socket_on_server = serv_sock;
		socket_file_local = sock_file;
		socket_tcp = sock_tcp;
		used = true;
		dimension = 0;
	}
};


struct download_file
{
	bool used;
	string name;
	int socket_on_server;
	int socket_file_local;
	int socket_tcp;
	long long dimension;
	long long real_dim;
	download_file()
	{
		used = false;
		dimension = 0;
	}
	download_file(char nume[], int serv_sock, int sock_file, int sock_tcp)
	{
		name = string(nume);
		socket_on_server = serv_sock;
		socket_file_local = sock_file;
		socket_tcp = sock_tcp;
		used = true;
		dimension = 0;
	}
};


vector<upload_file> uploading;
vector<download_file> downloading;


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


// functie ce afiseaza mesajul pentru un cod de succes
string afiseaza_ok (int ok_code)
{

	switch (ok_code) {
	case 1: return  string("Autentificare cu SUCCES");


	}
	return "";
}

// functie ce transforma un numar in string
string number_to_string(long long  nr)
{
	char number[33];


	sprintf(number, "%lld", nr);

	string s(number);
	return s;
}
// functie ce scrie in fisier si in output
void print_message(string s)
{
	cout << s << endl << flush;
	g << s << endl << flush;
}
// functie ce scrie in fisier
void print_to_file (string s)
{
	g << s << endl << flush;
}
// functie ce afiseaza la stdin
void print_to_stdin (string s)
{
	cout << s << endl << flush;
}

// functie ce afiseaza promptul userului
void print_user_prompt(int code)
{

	if (logged_in && code != NEAUTENTIFICAT)
	{
		cout << current_user << " " << flush;
		g << current_user << " " << flush;
	}
	else
	{
		cout << "$" << flush;
		g << "$" << flush;
	}
}
// functie de afisare a erorii
void print_error(int cod_eroare)
{
	string s = "";
	s = s + number_to_string(cod_eroare);
	s = s + " " + afiseaza_eroare(cod_eroare);
	print_message(s);
	print_user_prompt(cod_eroare);
}

// functie ce afiseaza un mesaj in functie de codul de succes
void print_ok(int cod_succes)
{
	if (cod_succes == IESIRE_SISTEM_SERVER)
		return;
	if (cod_succes == IESIRE_SISTEM)
		return;
	if (cod_succes ==  FISIER_STERS)
		return;
	if (cod_succes == FISIER_PARTAJAT)
		return;
	if (cod_succes == FISIER_PRIVAT)
		return;
	if (cod_succes == ACCESS_UPLOAD || cod_succes == ACCESS_DOWNLOAD)
	{
		print_user_prompt(cod_succes);
		return;
	}
	if (cod_succes == TRIMITE_STRING )
	{
		print_user_prompt(_SUCCES);
		return ;
	}
	string s = "";
	s = s + number_to_string(cod_succes);
	s = s + " " + afiseaza_ok(cod_succes) + '\n';
	print_message(s);

	print_user_prompt(cod_succes);
}



void error(const char *msg)
{
	perror(msg);
	exit(1);
}

// functie ce verifica daca exista fisier
long long exist_file(char file[])
{
	struct stat st;
	if (stat(file, &st) != 0)
	{
		return 0;
	}
	return st.st_size;
}

// functie ce determina daca o comanda este valida
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
	if (command == "upload")
		return 1;
	if (command == "download")
		return 1;
	if (command == "share")
		return 1;
	if (command == "unshare")
		return 1;
	if (command == "delete")
		return 1;
	if (command == "quit")
		return 1;
	return 0;
}

// functie ce analizeaza o comanda 
int check_command (string command)
{

	stringstream sin(command);
	char instruction[BUFLEN];
	sin >> instruction;
	if (string(instruction) == "login")
	{

		if (logged_in == true)
		{
			print_error(SESIUNE_DESCHISA);
			return 0;
		}

		return 1;

	}
	if (string(instruction) == "quit") return 1;
	// pentru restul functiilor trebuie sa fie logat
	if (logged_in == false)
	{
		print_error(NEAUTENTIFICAT);
		return 0;
	}
	if (string(instruction) == "logout")
	{
		logged_in = false;
		cout << "$" << flush;
		g << "$" << flush;

		return 1;
	}

	if (string(instruction) == "upload")
	{
		string file;
		getline(sin, file);
		// elimin spatiile albe de la inceput
		while (file.size() > 0 && ((file.at(0) == ' ') || (file.at(0) == '\t')) )
			file.erase(file.begin());
		if (file == "")
		{
			print_error(COMANDA_INVALIDA);
			return 0;
		}

		if (exist_file((char*)file.c_str()) == 0)
		{
			print_error(FISIER_NEEXISTENT);
			return 0;
		}


		for (vector<download_file>::iterator i = downloading.begin(); i != downloading.end(); ++i)
		{
			if (i->name == file)
			{
				print_error(FISIER_IN_TRANSFER);
				return 0;
			}
		}


		return 1;
	}

	if (string(instruction) == "download")
	{
		string user;
		sin >> user;
		if (user == "")
		{
			print_error(COMANDA_INVALIDA);
			return 0;
		}
		string file;
		getline(sin, file);// fisierul
		if (file == "")
		{
			print_error(COMANDA_INVALIDA);
			return 0;
		}
		return 1;
	}


	if (string(instruction) == "share" || string(instruction) == "unshare" || string(instruction) == "delete")
	{
		char temp[BUFLEN];
		strcpy(temp, command.c_str());
		char *p;
		p = strtok(temp, " \t");
		p = strtok(NULL, " \t");// trebuie sa existe numele fisierului
		if (!p)
		{
			print_error(COMANDA_INVALIDA);
			return 0;
		}
		return 1;


	}


	return 1;

}
void analyse_and_send_command( char command[], int len, int from_user)
{
	stringstream sin(command + 1);
	char instruction[BUFLEN];
	sin >> instruction;
	if (valid_command(string(instruction)) == 0) // analiza comenzii
	{
		print_error(COMANDA_INVALIDA);
		return ;

	}

	if (check_command(string(command + 1)) == 0)// analiza semantica ca comenzii
		return ;


	int sended;
	command[0] = from_user;
	sended = send(sockfd, command, len, 0);

	if (sended < 0)
		error("ERROR writing to socket");
}


// uploadez buff de lungime len
void upload(char buffer[], int len)
{
	int socket_server;
	memcpy(&socket_server, buffer, sizeof(int));
	char nume_fisier[BUFLEN];
	stringstream sin(buffer + sizeof(int));

	string file;
	// citesc numele fisierului
	getline(sin, file);
	// elimin spatiile albe de la inceput
	while (file.size() > 0 && ((file.at(0) == ' ') || (file.at(0) == '\t')) )
		file.erase(file.begin());


	strcpy(nume_fisier, file.c_str());

	// sa nu fie in transfer
	for (vector<download_file>::iterator i = downloading.begin(); i != downloading.end(); ++i)
	{
		if (i->name == string(nume_fisier))
		{
			print_error(FISIER_IN_TRANSFER);
			return;
		}
	}



	// creez socket nou
	int newsocket;
	newsocket = socket(AF_INET, SOCK_STREAM, 0);
	if (newsocket < 0)
		error("ERROR opening socket");

	if (connect(newsocket, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) < 0)
		error("ERROR connecting");


	FD_SET(newsocket, &read_fds);
	if (newsocket > fdmax)
		fdmax = newsocket;
	// deschid fisierul
	int sursa = open(nume_fisier, O_RDONLY) ;
	// bag in vector
	uploading.push_back(upload_file(nume_fisier, socket_server, sursa , newsocket));
	char sir[BUFLEN] = {0};
	long long dimension = exist_file(nume_fisier);
	// trimit informatia la server
	sprintf(sir + 1, "u %d %lld", socket_server, dimension);
	sir[0] = FROM_STATION;
	int sended;
	sended = send(newsocket, sir, strlen(sir + 1) + 1 + 1, 0);
	if (sended < 0)
		error("ERROR writing to socket");
}
// downloadez informatia command si o scriu in fisier
void download(char command[], int len)
{
	stringstream sin(command);
	int fd_server;
	long long dimension;
	char nume_fisier[BUFLEN];
	string file;
	sin >> fd_server >> dimension; // socketul si dimensiunea
	getline(sin, file);
	// sterg spatiile albe
	while (file.size() > 0 && ((file.at(0) == ' ') || (file.at(0) == '\t')) )
		file.erase(file.begin());
	strcpy(nume_fisier, file.c_str());

	// creez noul fisier
	char temp[LENGHT];
	sprintf(temp, "<%d>_%s", pid, nume_fisier);
	strcpy(nume_fisier, temp);
	// verific sa nu fie in transfer
	for (vector<download_file>::iterator i = downloading.begin(); i != downloading.end(); ++i)
	{
		if (i->name == string(nume_fisier))
		{
			print_error(FISIER_IN_TRANSFER);
			return;
		}
	}

	// creez socket de upload
	int newsocket;
	newsocket = socket(AF_INET, SOCK_STREAM, 0);
	if (newsocket < 0)
		error("ERROR opening socket");

	if (connect(newsocket, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) < 0)
		error("ERROR connecting");

	// deschid fisierul si il creez
	int sursa = open(nume_fisier, O_RDWR | O_CREAT, 0777);
	if (sursa < 0)
	{
		perror("nu am putut sa descid fisierul in care sa scriu download");
		return;
	}
	// creez fisierul de download ca sa il bag in vectorul de download
	download_file x(nume_fisier, fd_server, sursa, newsocket);
	x.dimension = dimension;
	x.real_dim = 0;


	FD_SET(newsocket, &read_fds);
	if (newsocket > fdmax)
		fdmax = newsocket;


	downloading.push_back(x);
	char buffer[BUFLEN];
	buffer[0] = FROM_STATION;
	// trimit informatia la server
	sprintf(buffer + 1, "d %d %lld \n", fd_server, dimension);
	int sended;
	sended = send(newsocket, buffer, strlen(buffer + 1) + 1 + 1, 0);
	if (sended < 0)
		error("ERROR writing to socket");



}
// functie ce imi analizeaza raspunsurile pozitive de la server
void analyse_ok_response( char buffer[], int len, int cod_succes)
{
	if (cod_succes == AUTENTIFICARE_SUCCES)
	{
		// pentru afisarea promptului
		current_user = string(buffer );
		current_user =  current_user + ">";
		logged_in = true;
		return;
	}
	if (cod_succes == TRIMITE_STRING)
	{
		print_message(string(buffer));
	}
	if (cod_succes == ACCESS_UPLOAD )
	{
		upload(buffer, len);
	}

	if (cod_succes == ACCESS_DOWNLOAD)
	{
		download(buffer, len);
	}
	if  (cod_succes == FISIER_PARTAJAT)
	{
		print_message( number_to_string(cod_succes) + ' ' + string(buffer));
		print_user_prompt(_SUCCES);
	}
	if (cod_succes == FISIER_PRIVAT)
	{
		print_message(number_to_string(cod_succes - 1) + ' ' + string(buffer));
		print_user_prompt(_SUCCES);
	}

	if (cod_succes == FISIER_STERS)
	{
		print_message(number_to_string(cod_succes - 2) + ' ' + string(buffer) + '\n');
		print_user_prompt(_SUCCES);
	}

	if (cod_succes == IESIRE_SISTEM)
	{
		print_message("Parasire sistem...\n[Asteptati pentru eventualele fisiere in transfer]\n");
		FD_CLR(0, &read_fds);
		FD_CLR(sockfd, &read_fds);
		FD_CLR(sockfd, &tmp_fds);
		FD_CLR(0, &tmp_fds);

		close(sockfd);
	}

	if (cod_succes == IESIRE_SISTEM_SERVER)
	{
		print_message("Serverul se inchide...\n");

		char temp[BUFLEN];
		temp[0] =  FROM_USER;
		// confirm ca ma deconectez
		strcpy(temp + 1, "quit");

		int sended = send(sockfd, temp, strlen(temp + 1) + 1 + 1, 0);
		if (sended < 0)
			error("ERROR writing to socket");
	}


}

void analyse_error_response(int cod_eroare)
{
	if (cod_eroare == BRUTE_FORCE)
	{
		print_message(string("Closing"));

		close(sockfd);
		closed_conection = true;
	}
}
void analyse_server_response(char buffer[], int len)
{

	if (buffer[0] < 0)// cod de eroare
	{
		int cod_eroare;

		memcpy(&cod_eroare, buffer + 1, sizeof(int));
		print_error(cod_eroare);
		analyse_error_response(cod_eroare);
		return ;
	}
	else
	{
		// succes
		int cod_succes;
		memcpy(&cod_succes, buffer + 1, sizeof(int));
		analyse_ok_response(buffer + 1 + sizeof(int), len, cod_succes);
		print_ok(cod_succes);
	}
}

// receptioneaza mesaj de la server
void receive_from_server(int server_socket)
{

	char buffer[BUFLEN];
	int received;

	received = recv(server_socket, buffer, BUFLEN, 0);

	if (received < 0 )
	{
		error("received filed ");
		return ;
	}

	analyse_server_response(buffer, received);

}
// incer sa uploadez fisierul
void try_upload(upload_file &file)
{
	char buffer[BUFLEN];
	buffer[0] = FROM_STATION;// nu e mesaj de la utilizator
	buffer[1] = UPLOAD_MESSAGE;// tipul mesajului
	int copiat = read(file.socket_file_local, buffer + 2, BUFLEN - 2);
	file.dimension += copiat;
	if (copiat == 0) // s-a terminat fisierul
	{
		file.used = false;
		string s;
		char temp [LENGHT];
		sprintf(temp, "%lld", file.dimension);
		// afisez terminarea uploadului
		s = "\nUpload finished " + file.name + " " + string(temp) + " bytes\n";
		print_message(s);
		print_user_prompt(_SUCCES);
		// scot din lista de descriptori si inchid conexiunea
		FD_CLR(file.socket_tcp, &read_fds);
		close(file.socket_file_local);
		return;
	}
	int	sended = send(file.socket_tcp, buffer, copiat + 2, 0);
	if (sended < 0)
		error("ERROR writing to socket");

}
// functie ce imi scrie in fisier informatia descarcata
void write_to_file_download(download_file &file, char buffer[], int len)
{

	int scriu = len;
	if (file.dimension < scriu)
		scriu = file.dimension;

	int scris = write(file.socket_file_local, buffer, scriu);

	if (scris < 0)
		error("nu am reusit sa scriu in fisier write_to_file");
	file.real_dim += scris;
	file.dimension -= scris;

	if (file.dimension == 0)// s-a terminat de descarcat
	{

		file.used = false;
		string s;
		char temp [LENGHT];
		sprintf(temp, "%lld", file.real_dim);
		// afisez sfarsitul downloadului
		s = "\nDownload finished " + file.name + " " + string(temp) + " bytes\n";
		print_message(s);
		print_user_prompt(_SUCCES);
		// scot din lista de descriptori si inchid conexiunea
		FD_CLR(file.socket_tcp, &read_fds);
		close(file.socket_file_local);
		return ;
	}
	char temp[BUFLEN];
	temp[0] = FROM_STATION;// mesajul nu e de la utilizator
	sprintf(temp + 1, "o");
	int sended = send(file.socket_tcp, temp, strlen(temp + 1) + 1, 0);
	if (sended == 0)
		error("nu am putut sa dau la server");

}

int main(int argc, char *argv[])
{


	if (argc < 3)
	{
		cout << " Few client arguments\n";
		exit(0);
	}
	// deschid fisierul
	char nume_fisier[LENGHT];
	sprintf(nume_fisier, "client-<%d>.log", getpid());
	g.open(nume_fisier, ios::out);


	char buffer[BUFLEN];

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0)
		error("ERROR opening socket");

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(atoi(argv[2]));
	inet_aton(argv[1], &serv_addr.sin_addr);


	if (connect(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) < 0)
		error("ERROR connecting");

	// introduc socketii pe care ascult initial
	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);
	FD_SET(sockfd, &read_fds);
	FD_SET(0, &read_fds);

	fdmax = sockfd;
	cout << "$" << flush;
	g << "$" << flush;

	while (1) {

		tmp_fds = read_fds;
		if (select(fdmax + 1, &tmp_fds, NULL, NULL, NULL) == -1)
			error("ERROR in select");

		int i;
		if (FD_ISSET(0, &tmp_fds)) {
			cin.getline(buffer + 1 , BUFLEN - 2);
			print_to_file(string(buffer + 1));
			if (strlen(buffer + 1) == 0) // este un enter
				print_user_prompt(_SUCCES);
			else
			{
				// analizez informatia
				analyse_and_send_command(buffer, strlen(buffer + 1) + 1 + 1, FROM_USER);
			}
		}



		if (FD_ISSET(sockfd, &tmp_fds))
		{
			// am primit de la server infonatii
			receive_from_server(sockfd);
			if (closed_conection)
				return -1;

		}
		for ( i = 0; i < (int)uploading.size() ; i++)
		{
			// am primit fisier de incarcat

			if (FD_ISSET(uploading[i].socket_tcp, &tmp_fds))
			{
				char temp[BUFLEN];
				int primit = recv(uploading[i].socket_tcp, temp, BUFLEN, 0); // receive ack
				if (primit < 0)
				{
					perror("Not working");
				}
				if (uploading[i].used)
				{

					try_upload(uploading[i]);

				}
			}
		}
		// sterg fisierele care s-au terminat
		for (i = 0; i < (int)uploading.size(); i++)
		{
			if (uploading[i].used == false)
				uploading.erase(uploading.begin() + i), i--;
		}
		// am primit fisiere de descarcat 
		for (i = 0 ; i < (int)downloading.size(); i++)
		{

			if (downloading[i].used && FD_ISSET(downloading[i].socket_tcp, &tmp_fds))
			{
				char temp[BUFLEN];
				int primit = recv(downloading[i].socket_tcp, temp, BUFLEN, 0); // receive ack
				if (primit < 0)
				{
					perror("Not working");
				}
				if (temp[0] == FROM_STATION && temp[1] ==  DOWNLOAD_MESSAGE)
				{
					write_to_file_download(downloading[i], temp + 2, primit - 2);
				}

			}

		}
		// sterg fisierele ce s-au descarcat
		for (i = 0; i < (int)downloading.size(); i++)
		{
			if (downloading[i].used == false)
				downloading.erase(downloading.begin() + i), i--;
		}

		// verific daca trebuie sa inchid conexiunea
		if ( (FD_ISSET (0, &read_fds) == 0) && uploading.size() == 0 && downloading.size() == 0)
		{
			g.close();
			return 0;
		}




	}
	g.close();
	close(sockfd);
	return 0;
}


