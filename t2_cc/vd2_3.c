#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>

#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

sqlite3 *db;



struct ServerWorker {
    int socketClient;
    struct Server *server;
    char login[1024];
};

struct Server {
    int serverPort;
    struct ServerWorker workerList[10];
    int workerCount;
};

// Structura pentru lista de utilizatori online
struct OnlineUser {
    int socketClient;
    char login[1024];
};

// Function declarations
void openDatabase();
void closeDatabase();
int callback(void *NotUsed, int argc, char **argv, char **azColName);
void createUserTable();

void addUser(const char *username, const char *password);
int checkUser(const char *username, const char *password);

void addOnlineUser(const char *login, int socketClient);
void removeOnlineUser(const char *login);
void sendToAllOnlineUsers(const char *message);
void displayOnlineUsers(struct ServerWorker *worker);
void handleLogin(struct ServerWorker *worker, const char *buffer);
void handleAdd(struct ServerWorker *worker, const char *buffer);
void handleMsg(struct ServerWorker *worker, const char *buffer);
void handleGet(struct ServerWorker *worker, const char *buffer);
void handlePost(struct ServerWorker *worker, const char *buffer);
void handlePut(struct ServerWorker *worker, const char *buffer);
void handlePatch(struct ServerWorker *worker, const char *buffer);
void handleDelete(struct ServerWorker *worker, const char *buffer);
void *handleClient(void *arg);
int main();

struct OnlineUser onlineUsers[10]; // Ajustați dimensiunea listei în funcție de necesități
int onlineUserCount = 0;

void openDatabase() {
    int rc = sqlite3_open("c.db", &db);
    if (rc) {
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
        exit(0);
    } else {
        fprintf(stderr, "Opened database successfully\n");
    }
}

void closeDatabase() {
    sqlite3_close(db);
}

int callback(void *NotUsed, int argc, char **argv, char **azColName) {
    int i;
    for (i = 0; i < argc; i++) {
        printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
    }
    printf("\n");
    return 0;
}


void createUserTable() {
    char *sql = "CREATE TABLE USERS(" \
                "USERNAME TEXT PRIMARY KEY NOT NULL," \
                "PASSWORD TEXT NOT NULL);";

    char *errMsg = 0;
    int rc = sqlite3_exec(db, sql, callback, 0, &errMsg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", errMsg);
        sqlite3_free(errMsg);
    } else {
        fprintf(stdout, "Table created successfully\n");
    }
}



void addOnlineUser(const char *login, int socketClient) {
    if (onlineUserCount < 10) {
        onlineUsers[onlineUserCount].socketClient = socketClient;
        strcpy(onlineUsers[onlineUserCount].login, login);
        onlineUserCount++;
    }
}

void removeOnlineUser(const char *login) {
    for (int i = 0; i < onlineUserCount; i++) {
        if (strcmp(onlineUsers[i].login, login) == 0) {
            // Găsit utilizatorul, îl eliminăm din lista online
            for (int j = i; j < onlineUserCount - 1; j++) {
                onlineUsers[j] = onlineUsers[j + 1];
            }
            onlineUserCount--;
            break;
        }
    }
}

// Funcție pentru a trimite un mesaj la toți utilizatorii conectați
void sendToAllOnlineUsers(const char *message) {
    for (int i = 0; i < onlineUserCount; i++) {
        int clientSocket = onlineUsers[i].socketClient;
        write(clientSocket, message, strlen(message));
    }
}

void displayOnlineUsers(struct ServerWorker *worker) {
    char msg[1024];
    strcpy(msg, "Online users: ");
    int onlineCount = 0;
    
    for (int i = 0; i < onlineUserCount; i++) {
        // Verificăm dacă utilizatorul este logat
        for (int j = 0; j < worker->server->workerCount; j++) {
            if (strcmp(onlineUsers[i].login, worker->server->workerList[j].login) == 0) {
                if (onlineCount == 0) {
                    strcat(msg, onlineUsers[i].login);
                } else {
                    strcat(msg, ", ");
                    strcat(msg, onlineUsers[i].login);
                }
                onlineCount++;
                break;
            }
        }
    }
    
    strcat(msg, "\n");
    write(worker->socketClient, msg, strlen(msg));
}

int checkUser(const char *username, const char *password) {
    sqlite3_stmt *stmt;
    const char *sql = "SELECT * FROM USERS WHERE USERNAME = ? AND PASSWORD = ?;";
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", sqlite3_errmsg(db));
        return -1; // Returnăm -1 în caz de eroare
    }

    // Bind username și password la interogare
    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, password, -1, SQLITE_STATIC);

    // Executăm interogarea
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        // Utilizatorul a fost găsit în baza de date
        sqlite3_finalize(stmt);
        return 1; // Returnăm 1 pentru utilizator găsit
    } else if (rc == SQLITE_DONE) {
        // Nu s-a găsit niciun utilizator cu acest nume și parolă
        sqlite3_finalize(stmt);
        return 0; // Returnăm 0 pentru utilizator negăsit
    } else {
        // Eroare în timpul execuției interogării
        fprintf(stderr, "SQL error: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        return -1; // Returnăm -1 în caz de eroare
    }
}

void addUser(const char *username, const char *password) {
    sqlite3_stmt *stmt;
    char *sql = "INSERT INTO USERS (USERNAME, PASSWORD) VALUES (?, ?);";
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", sqlite3_errmsg(db));
    }

    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, password, -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "SQL error: %s\n", sqlite3_errmsg(db));
    } else {
        fprintf(stdout, "User added successfully\n");
    }

    sqlite3_finalize(stmt);
}

void handleLogin(struct ServerWorker *worker, const char *buffer) {
    char login[256];
    char password[256];
    int argsCount = 0; // Initializăm cu 0

    // Copiem buffer-ul într-o variabilă temporară
    char *bufferCopy = strdup(buffer);
    if (bufferCopy == NULL) {
        // Tratează cazul în care nu s-a putut aloca memorie pentru copie
        return;
    }

    // Extragem login-ul și parola din copia buffer-ului
    char *token = strtok(bufferCopy, " ");
    while (token != NULL) {
        argsCount++; // Incrementăm argsCount pentru fiecare cuvânt găsit

        if (argsCount == 2) {
            strcpy(login, token); // Copiem login-ul în variabila login
        } else if (argsCount == 3) {
            // Copiem parola în variabila password, excludând caracterele cu cod ASCII 10 și 13
            int j = 0;
            for (int i = 0; token[i] != '\0'; i++) {
                if (token[i] != 10 && token[i] != 13) {
                    password[j++] = token[i];
                }
            }
            password[j] = '\0'; // Terminăm șirul de caractere
        }

        token = strtok(NULL, " "); // Continuăm să căutăm următorul cuvânt
    }

    free(bufferCopy); // Eliberăm memoria alocată pentru copie

    


    if (argsCount == 3) {
    	
    	// Print each character of password as ASCII number
    	//printf("Caracterele din parola sub formă de numere ASCII:\n");
    //for (int i = 0; i < strlen(password); i++) {
        //printf("%c: %d\n", password[i], password[i]);
    //}
    
    	int userExists = checkUser(login, password);
    	
        if (userExists == 1) {
            
    	
            char msg[1024];
            strcpy(msg, "ok login\n");
            write(worker->socketClient, msg, strlen(msg));

            // Adăugăm utilizatorul în lista online
            addOnlineUser(login, worker->socketClient);

            strcpy(worker->login, login);
            printf("User logged in successfully: %s\n", login);

            // Trimitem mesajul "online" către toți utilizatorii conectați
            char *onlineMsg = (char *)malloc(strlen("online ") + strlen(login) + 2); // 2 for '\n' and null terminator
            snprintf(onlineMsg, strlen("online ") + strlen(login) + 2, "online %s\n", login);
            sendToAllOnlineUsers(onlineMsg);
            free(onlineMsg);

            // Afișăm utilizatorii online după "ok login"
            displayOnlineUsers(worker);
        } else if (userExists == 0) {
            char msg[1024];
            strcpy(msg, "error login\n");
            write(worker->socketClient, msg, strlen(msg));
        }
    } else {
        char msg[1024];
        strcpy(msg, "error database\n");
        write(worker->socketClient, msg, strlen(msg));
    }
}


void handleAdd(struct ServerWorker *worker, const char *buffer) {
    char loginUsername[256];
    char password[256];
    int argsCount = 0; // Initializăm cu 0
    
    int userExists = checkUser(loginUsername, password);

    // Copiem buffer-ul într-o variabilă temporară
    char *bufferCopy = strdup(buffer);
    if (bufferCopy == NULL) {
        // Tratează cazul în care nu s-a putut aloca memorie pentru copie
        return;
    }

    // Extragem login-ul și parola din copia buffer-ului
    char *token = strtok(bufferCopy, " ");
    while (token != NULL) {
        argsCount++; // Incrementăm argsCount pentru fiecare cuvânt găsit

        if (argsCount == 2) {
            strcpy(loginUsername, token); // Copiem login-ul în variabila login
        } else if (argsCount == 3) {
            // Copiem parola în variabila password, excludând caracterele cu cod ASCII 10 și 13
            int j = 0;
            for (int i = 0; token[i] != '\0'; i++) {
                if (token[i] != 10 && token[i] != 13) {
                    password[j++] = token[i];
                }
            }
            password[j] = '\0'; // Terminăm șirul de caractere
        }

        token = strtok(NULL, " "); // Continuăm să căutăm următorul cuvânt
    }

    free(bufferCopy); // Eliberăm memoria alocată pentru copie

    

    if (argsCount == 3) {
    	// Verificăm dacă utilizatorul există deja în baza de date
        int userExists = checkUser(loginUsername, password);
        if (!userExists) {
    		addUser(loginUsername, password);
    		char msg[1024];
    		strcpy(msg, "ok user added\n");
    		write(worker->socketClient, msg, strlen(msg));
	} else {
    		char msg[1024];
    		strcpy(msg, "error user exists\n");
    		write(worker->socketClient, msg, strlen(msg));
	}
     }
     else {
            char msg[1024];
            strcpy(msg, "wrong add command\n");
            write(worker->socketClient, msg, strlen(msg));
        }
}

//primeste un sir de tip caractere/caractere/caractere
void handleMsg(struct ServerWorker *worker, const char *buffer){

     printf("Buffer primit: %s\n", buffer); // Afișează buffer-ul primit

    char matrix[100][100]; // Matricea în care vom salva cuvintele
    int row = 0; // Indicele rândului în matrice

    // Copiem buffer-ul într-o variabilă temporară și eliminăm caracterele cu codurile ASCII 10 și 13
    char *bufferCopy = (char *)malloc(strlen(buffer) + 1); // +1 pentru terminatorul NULL
    if (bufferCopy == NULL) {
        // Tratează cazul în care nu s-a putut aloca memorie pentru copie
        return;
    }

    int j = 0;
    for (int i = 0; buffer[i] != '\0'; i++) {
        if (buffer[i] != 10 && buffer[i] != 13) { // Excludem caracterele cu codurile ASCII 10 și 13
            bufferCopy[j++] = buffer[i];
        }
    }
    bufferCopy[j] = '\0'; // Terminăm șirul de caractere
        
    char *token = strtok(bufferCopy, "/");
    while (token != NULL) {
        strcpy(matrix[row], token); // Salvăm cuvântul în matrice
        printf("%s\n", matrix[row]); // Afisăm cuvântul (pentru demo)
        row++; // Trecem la următorul rând în matrice
        token = strtok(NULL, "/"); // Continuăm să căutăm următorul cuvânt
    }
    
    //  Spargem matrix[0] după caracterul " "
    char *wordToken = strtok(matrix[0], " ");
    int wordIndex = 0;
    while (wordToken != NULL) {
        strcpy(matrix[wordIndex++], wordToken);
        wordToken = strtok(NULL, " ");
    }
    
    //avem cuvant extras pe cate o linie in matricea matrix
    //cuvantul nu se termina cu \0
    
    
    // Afisam continutul matricei
    printf("Continutul matricei:\n");
    for (int i = 0; i < row; i++) {
    	printf("%s\n", matrix[i]);
    }
    
    free(bufferCopy);
    
   
    
    
    if( (int)strlen(matrix[0]) == 3){
    	if(matrix[0][0] == 'm' && matrix[0][1] == 's' && matrix[0][2] == 'g'){
    		// Definim o matrice locală pentru a stoca utilizatorii online
        	char onlineUsersMatrix[100][256];
        	
        	// Curățare completă a matricei onlineUsersMatrix
		for (int i = 0; i < 100; i++) {
    			for (int j = 0; j < 256; j++) {
        			onlineUsersMatrix[i][j] = '\0'; // Setează fiecare caracter la '\0'
    			}
		}
		
        	int onlineCount = 0;

		char msg[256];
    
    		for (int i = 0; i < onlineUserCount; i++) {
        		// Verificăm dacă utilizatorul este logat
        		for (int j = 0; j < worker->server->workerCount; j++) {
            			if (strcmp(onlineUsers[i].login, worker->server->workerList[j].login) == 0) {
                			if (onlineCount == 0) {
                    				strcat(msg, onlineUsers[i].login);
                			} else {
                    				strcat(msg, " ");
                    				strcat(msg, onlineUsers[i].login);
                			}
                			onlineCount++;
                			break;
            			}
        		}
    		}
    		strcat(msg, "\n");
    		
    		char *token = strtok(msg, " "); // obține primul token până la primul spațiu

		// Iterăm până când nu mai sunt token-uri sau întâlnim \n
    		while (token != NULL && strcmp(token, "\n") != 0) {
        		strcpy(onlineUsersMatrix[onlineCount], token); // copiem token-ul în onlineUsersMatrix
        
        		// Adaugăm caracterul NULL la sfârșitul fiecărei linii dacă este cazul
        		if (onlineUsersMatrix[onlineCount][strlen(onlineUsersMatrix[onlineCount]) - 1] != '\0') {
            			onlineUsersMatrix[onlineCount][strlen(onlineUsersMatrix[onlineCount])] = '\0';
        		}
        	
        		onlineCount++; // incrementăm contorul pentru următoarea linie din onlineUsersMatrix
        		token = strtok(NULL, " "); // obținem următorul token
    		}
    
    		printf("Numărul de utilizatori online este: %d\n", onlineCount);
		
		// Afiseaza fiecare caracter din matricea onlineUsersMatrix, inclusiv terminatorul NULL
		for (int i = 0; i < onlineCount; i++) {
    			printf("Linia %d: ", i + 1); // Afiseaza numarul liniei
    			// Parcurge fiecare caracter din șirul onlineUsersMatrix[i]
    			printf("\n");
    			printf("Lungimea liniei %d: %zu\n", i + 1, strlen(onlineUsersMatrix[i]));
    			for (int j = 0; onlineUsersMatrix[i][j] != '\0'; j++) {
        			printf("%c", onlineUsersMatrix[i][j]); // Afiseaza caracterul
    			}
    			printf("\n"); // Adauga un nou rand dupa fiecare linie afisata
		}
		
		
		// Creăm matricea pentru stocarea cuvintelor
    		char wordsMatrix[11][256];
    		int wordCount = 0; // Inițializăm wordCount cu 0
    		int ok_3 = 0; 
    		int ok_4 = 0;
    
    		// Parcurgem matricea onlineUsersMatrix pentru a identifica și extrage cuvintele
    for (int i = 0; i < onlineCount; i++) {
        if (strlen(onlineUsersMatrix[i]) > 0) {
           if (strlen(onlineUsersMatrix[i]) == 2 && onlineUsersMatrix[i][0] == 'o' && onlineUsersMatrix[i][1] == 'k' && ok_3 == 0) 			
           {
            				//intalnim pentru prima oara ok pe o linie in matrice deci facem ok_3 = 1
            				ok_3 = 1;
                			continue; // Dacă întâlnim "ok", trecem la următoarea linie
            } 
            else if (ok_3 == 1 && ok_4 == 0) 
            {
            	// Gaseste prima linie in care avem cuvantul login + numele de utilizator deci facem ok_4 =1
		ok_4 = 1;

// Dacă întâlnim "login", salvăm restul cuvântului în wordsMatrix
char word[256];
int foundFirstLetter = 0; // Flag pentru a urmări dacă am găsit primul caracter literă

for (int j = 0; onlineUsersMatrix[i][j] != '\0'; j++) {
    if (!foundFirstLetter) {
        // Verificăm dacă caracterul este o literă (în intervalul ASCII 65-90 sau 97-122)
        if ((onlineUsersMatrix[i][j] >= 65 && onlineUsersMatrix[i][j] <= 90) || (onlineUsersMatrix[i][j] >= 97 && onlineUsersMatrix[i][j] <= 122)) {
            		foundFirstLetter = 1; // Am găsit primul caracter literă
            		strcpy(word, onlineUsersMatrix[i] + j); // Copiem restul cuvântului în word de la poziția j
        	}
    	} else {
        // Verificăm dacă caracterul este o literă (în intervalul ASCII 65-90 sau 97-122)
        if ((onlineUsersMatrix[i][j] >= 65 && onlineUsersMatrix[i][j] <= 90) || (onlineUsersMatrix[i][j] >= 97 && onlineUsersMatrix[i][j] <= 122)) {
            				// Dacă am găsit primul caracter literă, copiem caracterele în word
            				strcat(word, &onlineUsersMatrix[i][j]); // Adăugăm caracterul în word
        			} else {
            				break; // Ieșim din buclă când întâlnim primul caracter care nu este literă
        			}
    			}
		}

		strcpy(wordsMatrix[wordCount], word); // Adăugăm cuvântul în wordsMatrix
		wordCount++; // Incrementăm contorul pentru următorul cuvânt
            
                
            } else {
                	// Dacă nu se potrivește nicio condiție specială, salvăm cuvântul întreg în wordsMatrix
                	strcpy(wordsMatrix[wordCount], onlineUsersMatrix[i]);
                	wordCount++; // Incrementăm contorul pentru următorul cuvânt
            	}
        }
    }

    		// Afișăm cuvintele extrase din onlineUsersMatrix
    		printf("Cuvintele extrase sunt:\n");
    		for (int i = 0; i < wordCount; i++) {
        		printf("%s\n", wordsMatrix[i]);
    		}
    		
    		
    		// Afișăm cuvintele extrase din onlineUsersMatrix
		printf("Cuvintele extrase sunt:\n");

		// Declaram matricea pentru stocarea cuvintelor unice
		char uniqueWords[11][256];
		int uniqueWordCount = 0; // Inițializăm uniqueWordCount cu 0

		for (int i = 0; i < wordCount; i++) {
    			if (i == 0) {
        			// Dacă i este 0, procesăm cuvântul și îl salvăm în noua matrice
        			char modifiedWord[256]; // Declaram un vector de caractere pentru a stoca cuvantul modificat
        			strcpy(modifiedWord, wordsMatrix[i] + 6); // Copiem restul cuvântului în modifiedWord

        			// Verificam daca cuvantul este deja in matricea de cuvinte unice
        			int found = 0;
        			for (int j = 0; j < uniqueWordCount; j++) {
            				if (strcmp(modifiedWord, uniqueWords[j]) == 0) {
                				found = 1; // Cuvantul a fost gasit in matricea de cuvinte unice
                				break;
            				}
        			}

        			if (!found) {
            				// Daca cuvantul nu a fost gasit, il adaugam in matricea de cuvinte unice
            				strcpy(uniqueWords[uniqueWordCount], modifiedWord);
            				uniqueWordCount++; // Incrementam contorul pentru urmatorul cuvant unic
        			}
    			} else {
        			// Altfel, procesăm cuvântul și îl salvăm în noua matrice fără a verifica duplicarea
        			// Verificam daca cuvantul este deja in matricea de cuvinte unice
        			int found = 0;
        			for (int j = 0; j < uniqueWordCount; j++) {
            				if (strcmp(wordsMatrix[i], uniqueWords[j]) == 0) {
                				found = 1; // Cuvantul a fost gasit in matricea de cuvinte unice
                				break;
            				}
        			}

        			if (!found) {
            				// Daca cuvantul nu a fost gasit, il adaugam in matricea de cuvinte unice
            				strcpy(uniqueWords[uniqueWordCount], wordsMatrix[i]);
            				uniqueWordCount++; // Incrementam contorul pentru urmatorul cuvant unic
        			}
    			}
		}
		

		// Afișăm cuvintele unice extrase
		printf("Cuvintele extrase unice sunt:\n");
		for (int i = 0; i < uniqueWordCount; i++) {
			printf("Lungimea cuvântului %d: %zu\n", i + 1, strlen(uniqueWords[i]));
    			printf("%s\n", uniqueWords[i]);
		}
    		
    		
    		//verificam daca matrix[1] este inclus ca si cuvant in una din liniile din uniqueWords
		
		int ok_2 = 0;
    
    		// Verificăm dacă matrix[1] este inclus ca și cuvânt în una din liniile din uniqueWords
    		for (int i = 0; i < uniqueWordCount; i++) {
        		if (strstr(uniqueWords[i], matrix[1]) != NULL) {
           			ok_2 = 1;
            			break; // Ieșim din buclă dacă am găsit o potrivire
        		}
    		}
		
		
		
		//userul se afla in lista de useri online
		if(ok_2 == 1){
		 	// Trimiterea mesajului către utilizatorul din matrix[1]
    			for (int i = 0; i < worker->server->workerCount; i++) {
        			if (strcmp(worker->server->workerList[i].login, matrix[1]) == 0) {
            				// Am găsit socket-ul asociat utilizatorului
            				write(worker->server->workerList[i].socketClient, matrix[2], strlen(matrix[2]));
            				printf("Mesajul a fost trimis cu succes către %s.\n", matrix[1]);
            				break;
        			}
    			}
		}
		else{
			// Utilizatorul este offline, trimitem mesajul utilizatorului care a apelat funcția
    			write(worker->socketClient, "Utilizatorul către care doriți să trimiteți mesajul este offline.\n", strlen("Utilizatorul către care doriți să trimiteți mesajul este offline.\n"));
		}
    		
    	}
    }
    
    
}

//primeste un sir de tip caractere/caractere/caractere
void handleGet(struct ServerWorker *worker, const char *buffer) {
    char matrix[100][100]; // Matricea în care vom salva cuvintele
    int row = 0; // Indicele rândului în matrice

    // Copiem buffer-ul într-o variabilă temporară și eliminăm caracterele cu codurile ASCII 10 și 13
    char *bufferCopy = (char *)malloc(strlen(buffer) + 1); // +1 pentru terminatorul NULL
    if (bufferCopy == NULL) {
        // Tratează cazul în care nu s-a putut aloca memorie pentru copie
        return;
    }

    int j = 0;
    for (int i = 0; buffer[i] != '\0'; i++) {
        if (buffer[i] != 10 && buffer[i] != 13) { // Excludem caracterele cu codurile ASCII 10 și 13
            bufferCopy[j++] = buffer[i];
        }
    }
    bufferCopy[j] = '\0'; // Terminăm șirul de caractere
    
    if (strcmp(bufferCopy, "users") == 0) {
    	// Afișăm utilizatorii online
        displayOnlineUsers(worker);
    }
    
    char *token = strtok(bufferCopy, "/");
    while (token != NULL) {
        strcpy(matrix[row], token); // Salvăm cuvântul în matrice
        printf("%s\n", matrix[row]); // Afisăm cuvântul (pentru demo)
        row++; // Trecem la următorul rând în matrice
        token = strtok(NULL, "/"); // Continuăm să căutăm următorul cuvânt
    }

    if (strcmp(matrix[0], "users") == 0 && strcmp(matrix[1], "online") == 0) {
    	// Afișăm utilizatorii online
        displayOnlineUsers(worker);
    }
    
    // executa: users/status/{id_user}
    if (strcmp(matrix[0], "users") == 0 && strcmp(matrix[1], "status") == 0) {
    	// Verificăm dacă utilizatorul specificat (matrix[2]) este online
        int userOnline = 0; // 0 pentru offline, 1 pentru online
        
        for (int i = 0; i < onlineUserCount; i++) {
            if (strcmp(onlineUsers[i].login, matrix[2]) == 0) {
                userOnline = 1;
                break;
            }
        }

        // Construim mesajul de răspuns către client
        char response[1024];
        snprintf(response, sizeof(response), "%s : %s\n", matrix[2], (userOnline ? "online" : "offline"));
        
        // Trimitem răspunsul înapoi către client
        write(worker->socketClient, response, strlen(response));
    }
}

//primeste un sir de tip caractere/caractere/caractere
void handlePost(struct ServerWorker *worker, const char *buffer) {
    char matrix[100][100]; // Matricea în care vom salva cuvintele
    int row = 0; // Indicele rândului în matrice

    // Copiem buffer-ul într-o variabilă temporară și eliminăm caracterele cu codurile ASCII 10 și 13
    char *bufferCopy = (char *)malloc(strlen(buffer) + 1); // +1 pentru terminatorul NULL
    if (bufferCopy == NULL) {
        // Tratează cazul în care nu s-a putut aloca memorie pentru copie
        return;
    }

    int j = 0;
    for (int i = 0; buffer[i] != '\0'; i++) {
        if (buffer[i] != 10 && buffer[i] != 13) { // Excludem caracterele cu codurile ASCII 10 și 13
            bufferCopy[j++] = buffer[i];
        }
    }
    bufferCopy[j] = '\0'; // Terminăm șirul de caractere


    
    if (strcmp(bufferCopy, "quit") == 0){
    	// Deconectare
        close(worker->socketClient);

        // Eliminăm utilizatorul din lista online
        if (worker->login[0] != '\0') {
            removeOnlineUser(worker->login);
        }

        // Eliberăm resursele și ieșim din thread
        pthread_exit(NULL);
    }
    
    else{
    	char *token = strtok(bufferCopy, "/");

    	while (token != NULL) {
        	strcpy(matrix[row], token); // Salvăm cuvântul în matrice
        	printf("%s\n", matrix[row]); // Afisăm cuvântul (pentru demo)
        	row++; // Trecem la următorul rând în matrice
        	token = strtok(NULL, "/"); // Continuăm să căutăm următorul cuvânt
    	}
    
    	// Verificăm dacă pe prima linie avem cuvântul "login"
    	if (strcmp(matrix[0], "login") == 0) {
    		char formatted_buffer[2048]; // Am declarat un buffer pentru stocarea șirului formatat

    		// Concatenăm matrix[0], matrix[1] și matrix[2] cu un spațiu între ele în buffer
    		snprintf(formatted_buffer, sizeof(formatted_buffer), "%s %s %s", matrix[0], matrix[1], matrix[2]);

    		printf("Conținutul buffer-ului înainte de apelul funcției handleLogin: %s\n", formatted_buffer);

    		handleLogin(worker, formatted_buffer);
   	}
   	// Verificăm dacă pe prima linie avem cuvântul "add"
    	if (strcmp(matrix[0], "add") == 0) {
    		char formatted_buffer[2048]; // Am declarat un buffer pentru stocarea șirului formatat

    		// Concatenăm matrix[0], matrix[1] și matrix[2] cu un spațiu între ele în buffer
    		snprintf(formatted_buffer, sizeof(formatted_buffer), "%s %s %s", matrix[0], matrix[1], matrix[2]);

    		printf("Conținutul buffer-ului înainte de apelul funcției handleLogin: %s\n", formatted_buffer);

    		handleAdd(worker, formatted_buffer);
   	}
   	
   	// Verificăm dacă pe prima linie avem cuvântul "msg"
    	if (strcmp(matrix[0], "msg") == 0) {
    		char formatted_buffer[2048]; // Am declarat un buffer pentru stocarea șirului formatat

    		// Concatenăm matrix[0], matrix[1] și matrix[2] cu un spațiu între ele în buffer
    		snprintf(formatted_buffer, sizeof(formatted_buffer), "%s %s %s", matrix[0], matrix[1], matrix[2]);

    		printf("Conținutul buffer-ului înainte de apelul funcției handleLogin: %s\n", formatted_buffer);


    		handleMsg(worker, formatted_buffer);
   	}
   	
   	// Eliberăm memoria alocată pentru copia buffer-ului
    	free(bufferCopy);
   }
}

//primeste un sir de tip caractere/caractere/caractere/caractere
void handlePut(struct ServerWorker *worker, const char *buffer) {
    char matrix[100][100]; // Matricea în care vom salva cuvintele
    int row = 0; // Indicele rândului în matrice

    // Copiem buffer-ul într-o variabilă temporară și eliminăm caracterele cu codurile ASCII 10 și 13
    char *bufferCopy = (char *)malloc(strlen(buffer) + 1); // +1 pentru terminatorul NULL
    if (bufferCopy == NULL) {
        // Tratează cazul în care nu s-a putut aloca memorie pentru copie
        return;
    }

    int j = 0;
    for (int i = 0; buffer[i] != '\0'; i++) {
        if (buffer[i] != 10 && buffer[i] != 13) { // Excludem caracterele cu codurile ASCII 10 și 13
            bufferCopy[j++] = buffer[i];
        }
    }
    bufferCopy[j] = '\0'; // Terminăm șirul de caractere
    
    char *token = strtok(bufferCopy, "/");

    while (token != NULL) {
        strcpy(matrix[row], token); // Salvăm cuvântul în matrice
        printf("%s\n", matrix[row]); // Afisăm cuvântul (pentru demo)
        row++; // Trecem la următorul rând în matrice
        token = strtok(NULL, "/"); // Continuăm să căutăm următorul cuvânt	
    }
    
    // Verificăm dacă avem suficiente cuvinte în matrice pentru a construi variabilele cerute
    if (row >= 4) {
        char user_delete[200];
        sprintf(user_delete, "%s/%s", matrix[0], matrix[1]);

        char user_create[2048];
	sprintf(user_create, "add/%s/%s\n", matrix[2], matrix[3]);
	
	handleDelete(worker, user_delete);
	
	handlePost(worker, user_create);
        
    }

    // Eliberăm memoria alocată pentru copia buffer-ului
    free(bufferCopy);
    
}

//parola asociata userului se afla in password
void searchPasswordByUsername(char *username, char *password) {
    sqlite3_stmt *stmt;
    const char *sql = "SELECT PASSWORD FROM USERS WHERE USERNAME = ?";
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Error preparing statement: %s\n", sqlite3_errmsg(db));
        return;
    }

    // Bind the username parameter
    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);

    // Execute the query
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        // If a row is found, retrieve the password
        const unsigned char *pass = sqlite3_column_text(stmt, 0);
        strcpy(password, (const char *)pass);
    } else {
        // No row found for the given username
        strcpy(password, ""); // Set password to empty string
    }

    // Finalize the statement
    sqlite3_finalize(stmt);
}

//primeste un sir de tip caractere/caractere
void handlePatch(struct ServerWorker *worker, const char *buffer) {
    char matrix[100][100]; // Matricea în care vom salva cuvintele
    int row = 0; // Indicele rândului în matrice

    // Copiem buffer-ul într-o variabilă temporară și eliminăm caracterele cu codurile ASCII 10 și 13
    char *bufferCopy = (char *)malloc(strlen(buffer) + 1); // +1 pentru terminatorul NULL
    if (bufferCopy == NULL) {
        // Tratează cazul în care nu s-a putut aloca memorie pentru copie
        return;
    }

    int j = 0;
    for (int i = 0; buffer[i] != '\0'; i++) {
        if (buffer[i] != 10 && buffer[i] != 13) { // Excludem caracterele cu codurile ASCII 10 și 13
            bufferCopy[j++] = buffer[i];
        }
    }
    bufferCopy[j] = '\0'; // Terminăm șirul de caractere
    
    char *token = strtok(bufferCopy, "/");

    while (token != NULL) {
        strcpy(matrix[row], token); // Salvăm cuvântul în matrice
        printf("%s\n", matrix[row]); // Afisăm cuvântul (pentru demo)
        row++; // Trecem la următorul rând în matrice
        token = strtok(NULL, "/"); // Continuăm să căutăm următorul cuvânt	
    }
    
    // Verificăm dacă avem suficiente cuvinte în matrice pentru a construi variabilele cerute
    if (row >= 2) {
	
	 // Cautăm parola utilizatorului în baza de date
    	char old_user_password[100]; // Să presupunem că parola utilizatorului are maxim 100 de caractere
        searchPasswordByUsername(matrix[0], old_user_password); // Funcție pentru căutarea parolei după username
	
	char user_delete[200];
	sprintf(user_delete, "%s/%s", matrix[0], old_user_password);
	
	handleDelete(worker, user_delete);
	
	char user_create[2048];
	sprintf(user_create, "add/%s/%s\n", matrix[1], old_user_password);
	
	handlePost(worker, user_create);
        
    }

    // Eliberăm memoria alocată pentru copia buffer-ului
    free(bufferCopy);
}

//primeste un sir de tip caractere/caractere
void handleDelete(struct ServerWorker *worker, const char *buffer) {
    char matrix[100][100]; // Matricea în care vom salva cuvintele
    int row = 0; // Indicele rândului în matrice

    // Copiem buffer-ul într-o variabilă temporară și eliminăm caracterele cu codurile ASCII 10 și 13
    char *bufferCopy = (char *)malloc(strlen(buffer) + 1); // +1 pentru terminatorul NULL
    if (bufferCopy == NULL) {
        // Tratează cazul în care nu s-a putut aloca memorie pentru copie
        return;
    }

    int j = 0;
    for (int i = 0; buffer[i] != '\0'; i++) {
        if (buffer[i] != 10 && buffer[i] != 13) { // Excludem caracterele cu codurile ASCII 10 și 13
            bufferCopy[j++] = buffer[i];
        }
    }
    bufferCopy[j] = '\0'; // Terminăm șirul de caractere
    
    char *token = strtok(bufferCopy, "/");

    while (token != NULL) {
        strcpy(matrix[row], token); // Salvăm cuvântul în matrice
        printf("%s\n", matrix[row]); // Afisăm cuvântul (pentru demo)
        row++; // Trecem la următorul rând în matrice
        token = strtok(NULL, "/"); // Continuăm să căutăm următorul cuvânt	
    }
    
    // Verificăm dacă există un utilizator cu username = matrix[0] și password = matrix[1]
    int userExists = checkUser(matrix[0], matrix[1]);
    if (userExists == 1) {
        // Utilizatorul există în baza de date, deci îl ștergem
        // Construim șirul SQL pentru ștergere
        char *sql = "DELETE FROM USERS WHERE USERNAME = ? AND PASSWORD = ?;";
        sqlite3_stmt *stmt;
        int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
        if (rc != SQLITE_OK) {
            fprintf(stderr, "SQL error: %s\n", sqlite3_errmsg(db));
            return;
        }

        // Bind parametrii la interogare
        sqlite3_bind_text(stmt, 1, matrix[0], -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, matrix[1], -1, SQLITE_STATIC);

        // Executăm interogarea
        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE) {
            fprintf(stderr, "SQL error: %s\n", sqlite3_errmsg(db));
        } else {
            fprintf(stdout, "User deleted successfully\n");
            // Trimiterea unui mesaj către client că utilizatorul a fost șters cu succes
            char msg[1024];
            strcpy(msg, "ok user deleted\n");
            write(worker->socketClient, msg, strlen(msg));
        }

        sqlite3_finalize(stmt);
    } else if (userExists == 0) {
        // Utilizatorul nu există în baza de date
        printf("Utilizatorul nu există în baza de date.\n");
        // Trimiterea unui mesaj către client că utilizatorul nu a fost găsit
        char msg[1024];
        strcpy(msg, "error user not found\n");
        write(worker->socketClient, msg, strlen(msg));
    } else {
        // A apărut o eroare în timpul verificării utilizatorului
        printf("Eroare în timpul verificării utilizatorului.\n");
        // Trimiterea unui mesaj de eroare către client
        char msg[1024];
        strcpy(msg, "error database error\n");
        write(worker->socketClient, msg, strlen(msg));
    }
}

void *handleClient(void *arg) {
    struct ServerWorker *worker = (struct ServerWorker *)arg;
    int inputStream = worker->socketClient;
    char buffer[1024];
    int bytesRead;

    if (worker->server->workerCount == 1) {
        // Dacă suntem la al doilea client care se conectează
        // Afișăm utilizatorii online înainte de a verifica login-ul
        displayOnlineUsers(worker);
    }

    while ((bytesRead = read(inputStream, buffer, sizeof(buffer))) > 0) {
        buffer[bytesRead] = '\0';
        
        char buffer_copy[1024];
        
        strcpy(buffer_copy, buffer);
        
        printf("Conținutul lui buffer_copy: %s\n", buffer_copy);
        
        char buffer_copy_3[1024]; 
        
        // Căutăm al doilea caracter '/'
        int slashCount = 0;
        int charIndex = 0;
        while (buffer_copy[charIndex] != '\0') {
            if (buffer_copy[charIndex] == '/') {
                slashCount++;
                if (slashCount == 2) {
                    charIndex++;
                    break;
                }
            }
            charIndex++;
        }

        if (slashCount >= 2) {
            strcpy(buffer_copy_3, &buffer_copy[charIndex]); // Copiem partea după al doilea '/' în buffer_copy_3
 
        }
        
        
        

        char words[100][100]; // Matricea în care vom salva cuvintele
        int word_count = 0; // Numărul de cuvinte găsite

        // Folosim strtok() pentru a sparge buffer_copy în cuvinte
        char *token = strtok(buffer_copy, " ");
        while (token != NULL) {
            strcpy(words[word_count], token); // Salvăm cuvântul în matrice
            word_count++; // Incrementăm numărul de cuvinte găsite
            token = strtok(NULL, " "); // Continuăm să căutăm următorul cuvânt
        }
        
        // Căutăm al doilea caracter '/' în al doilea cuvânt (words[1])
    	//int slashCount_2 = 0;
    	//int charIndex_2 = 0;
    	//while (words[1][charIndex_2] != '\0') {
        	//if (words[1][charIndex_2] == '/') {
            		//slashCount_2++;
            		//if (slashCount_2 == 2) {
                		//charIndex_2++;
                		//break;
            		//}
        	//}
        	//charIndex_2++;
    	//}
        
        
        // Afisăm ce este după al doilea '/'
    	//if (slashCount_2 >= 2) {
        //	strcat( &words[1][charIndex_2], buffer_copy_3);
         	
        //}
    
    	printf("Conținutul lui words_1: %s\n", words[1]);
    
        // Afisam cuvintele gasite pe linii separate
        for (int i = 0; i < word_count; i++) {
            //daca suntem pe primul cuvant atunci
            if (i == 0){
                if(strcmp(words[i], "get") == 0){
                    handleGet(worker, words[1]);
                }
                if(strcmp(words[i], "post") == 0){
                    handlePost(worker, words[1]);
                }
                if(strcmp(words[i], "put") == 0){
                    handlePut(worker, words[1]);
                }
                if(strcmp(words[i], "patch") == 0) {
                    handlePatch(worker, words[1]);
                }
                if(strcmp(words[i], "delete") == 0){
                    handleDelete(worker, words[1]);
                }
            }
            
        }   
    }

    close(worker->socketClient);

    // Eliminăm utilizatorul din lista online
    if (worker->login[0] != '\0') {
        removeOnlineUser(worker->login);
    }

    // Eliberăm resursele și ieșim din thread
    pthread_exit(NULL);
}

int main() {

    openDatabase();
    createUserTable();

    addUser("guest", "guest");
    addUser("jim", "jim");
    addUser("andrei", "andrei");
    addUser("alex", "alex");

    struct Server server;
    server.serverPort = 8818;
    server.workerCount = 0;

    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(server.serverPort);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    bind(serverSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr));
    listen(serverSocket, 5);

    while (1) {
        printf("About to acknowledge client connection...\n");
        struct sockaddr_in clientAddr;
        socklen_t addrLen = sizeof(clientAddr);
        int socketClient = accept(serverSocket, (struct sockaddr *)&clientAddr, &addrLen);
        printf("Accepted connection from %s:%d\n", inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port));

        if (server.workerCount < 10) {
            struct ServerWorker *specialist = &server.workerList[server.workerCount];
            specialist->socketClient = socketClient;
            specialist->server = &server;
            specialist->login[0] = '\0';

            pthread_t thread;
            if (pthread_create(&thread, NULL, handleClient, specialist) != 0) {
                perror("pthread_create");
            }

            server.workerCount++;
        }
    }

    return 0;
}
