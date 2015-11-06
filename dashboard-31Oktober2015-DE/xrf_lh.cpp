
/* by KI4LKF */

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <string>
#include <set>
#include <map>
#include <utility>
#include <sys/sysinfo.h>

using namespace std;

static char DXRFD_VERSION[5] = "----";

#define VERSION "rev. 31.10.2015" /* ik5xmk */

// linked nodes
typedef set<string> linked_type;
static linked_type linked_list[5];  /* 0=A, 1=B, 2=C */
static linked_type::iterator linked_pos[5]; /* 0=A, 1=B, 2=C */
static char rptr_x[5][10];

// conected users
typedef set<string> connected_type;
static connected_type connected_list;
static connected_type::iterator connected_pos;

// last heard
typedef set<string> lh_type;
static lh_type lh_list;
static lh_type::reverse_iterator r_lh_pos;

static bool keep_running = true;
static int g2_sock = -1;
static unsigned char queryCommand[2048];
static struct sockaddr_in toLink;
static struct sockaddr_in fromLink;

static char temp_user[9];
static char *temp_user_p = NULL;

static bool srv_open(char *ip);
static void srv_close();
static void sigCatch(int signum);

/* signal catching function */
static void sigCatch(int signum)
{
   /* do NOT do any serious work here */
   if ((signum == SIGTERM) || (signum == SIGINT))
      keep_running = false;
   return;
}
      
static bool srv_open(char *ip)
{
   /* create our gateway socket */ 
   g2_sock = socket(PF_INET,SOCK_DGRAM,0);
   if (g2_sock == -1)
   {
      fprintf(stderr,"Failed to create gateway socket,errno=%d\n",errno);
      return false;
   }

   memset(&toLink,0,sizeof(struct sockaddr_in));
   toLink.sin_family = AF_INET;
   toLink.sin_addr.s_addr = inet_addr(ip);
   toLink.sin_port = htons(20001); // il programma comunica con il ref su questa porta

   return true;
}  

static void srv_close()
{
   if (g2_sock != -1)
      close(g2_sock);

   return;
}

int main(int argc, char **argv)
{
   fd_set fdset;
   struct timeval tv;
   socklen_t fromlen;
   int recvlen, days, hours, mins;
   short i = 0;
   unsigned short j;
   short k = -1;
   unsigned short max_index = 0;
   time_t init_rq;
   time_t tnow;
   short total_keepalive = 3;
   struct sigaction act; 
   struct sysinfo sys_info;
   unsigned char *ptr = NULL;
   char *date_time = NULL;
   struct tm *mytm = NULL;
   char temp_string[64];

   setvbuf(stdout, (char *)NULL, _IOLBF, 0);
   fprintf(stderr, "VERSION %s\n", VERSION);

   if (argc != 9) // era 5 in origine, i parametri partono da 0
   {
      fprintf(stderr, "Usage: ./xrf_lh yourPersonalCallsign yourXRFreflector description IPaddressOF_XRF YES_info PATH/xrfs.url Seconds Contacts\n");
      return 1;
   }

   tzset();

   act.sa_handler = sigCatch;
   sigemptyset(&act.sa_mask);
   act.sa_flags = SA_RESTART;
   if (sigaction(SIGTERM, &act, 0) != 0)
   {
      fprintf(stderr, "sigaction-TERM failed, error=%d\n", errno);
      return 1;
   }
   if (sigaction(SIGINT, &act, 0) != 0)
   {
      fprintf(stderr,"sigaction-INT failed, error=%d\n", errno);
      return 1;
   }

   if (!srv_open((char *)argv[4]))
   {
      fprintf(stderr, "srv_open() failed\n");
      return 1;
   }

/* by ik5xmk  */

   FILE *f_xrfs;
   int numriga;
   char mystr [100]; //legge 100 caratteri da ogni riga del file che contiene l'url della dashboard del xrf
   int NMAXURL = 20; //numero massimo di url gestiti
   char url_xrf [NMAXURL][100]; //righe contenenti url da 100 caratteri

   f_xrfs=fopen(argv[6],"r"); // apre xrfs.url prendendo il percorso dalla riga dei parametri passati
   if (f_xrfs==NULL) {
     fprintf(stderr, "Errore in apertura del file %s", argv[6]);
     return 1;
   }
   numriga = 0;
   while ((fgets(mystr, 100, f_xrfs) != NULL) && (numriga < NMAXURL)) {
     strcpy(url_xrf[numriga], mystr); //copia la riga letta dal file nel vettore
     numriga++;
   }
   fclose(f_xrfs);

   // variabili usate per i cicli e confronti seguenti
   int nn;
   int nnn;
   int NRIGHEURL = numriga; //mi serve per i successivi confronti, contiene il numero di url/xrf presenti (riga 0 e' commento)
   char mystr2 [6];

 /* end block */

   /* initiate login */
   fprintf(stderr,"Requesting connection...\n");
   queryCommand[0] = 5;
   queryCommand[1] = 0;
   queryCommand[2] = 24;
   queryCommand[3] = 0;
   queryCommand[4] = 1;

   sendto(g2_sock,(char *)queryCommand,5,0,
             (struct sockaddr *)&toLink,
             sizeof(struct sockaddr_in));

   fcntl(g2_sock,F_SETFL,O_NONBLOCK);

   time(&init_rq);
   while (keep_running)
   {
      FD_ZERO(&fdset);
      FD_SET(g2_sock, &fdset);
      tv.tv_sec = 1;
      tv.tv_usec = 0;
      (void)select(g2_sock + 1, &fdset,0,0,&tv);

      if (FD_ISSET(g2_sock, &fdset))
      {
         fromlen = sizeof(struct sockaddr_in);
         recvlen = recvfrom(g2_sock,(char *)queryCommand, 2048,
                         0,(struct sockaddr *)&fromLink,&fromlen);

         /*** check that the incoming IP = outgoing IP ***/
         if (fromLink.sin_addr.s_addr != toLink.sin_addr.s_addr)
            continue;
      
         if ((recvlen == 3) &&
             (queryCommand[0] == 3) &&
             (queryCommand[1] == 96) &&
             (queryCommand[2] == 0))
         {
            sendto(g2_sock,(char *)queryCommand,3,0,
                (struct sockaddr *)&toLink,
                sizeof(struct sockaddr_in));

            total_keepalive--;
            if (total_keepalive == 0)
               break;
         }
         else
         if ((recvlen == 5) &&
             (queryCommand[0] == 5) &&
             (queryCommand[1] == 0) &&
             (queryCommand[2] == 24) &&
             (queryCommand[3] == 0) &&
             (queryCommand[4] == 1))
         {
            fprintf(stderr,"Connected...\n");
            memset(queryCommand, ' ', 2048);
            queryCommand[0] = 28;
            queryCommand[1] = 192;
            queryCommand[2] = 4;
            queryCommand[3] = 0;

            memcpy(queryCommand + 4, argv[1], strlen(argv[1]));
            for (j = 11; j > 3; j--)
            {
               if (queryCommand[j] == ' ')
                  queryCommand[j] = '\0';
               else
                  break;
            }
            memset(queryCommand + 12, '\0', 8);
            memcpy(queryCommand + 20, "DV019999", 8);

            sendto(g2_sock,(char *)queryCommand,28,0,
                (struct sockaddr *)&toLink,
                sizeof(struct sockaddr_in));
         }
         else
         if ((recvlen == 8) &&
             (queryCommand[0] == 8) &&
             (queryCommand[1] == 192) &&
             (queryCommand[2] == 4) &&
             (queryCommand[3] == 0))
         {
            if ((queryCommand[4] == 79) &&
                (queryCommand[5] == 75) &&
                (queryCommand[6] == 82))
            {
               fprintf(stderr,"Login OK, requesting gateway info...\n\n");

               /* request version */
               queryCommand[0] = 0x04;
               queryCommand[1] = 0xc0;
               queryCommand[2] = 0x03;
               queryCommand[3] = 0x00;
               sendto(g2_sock,(char *)queryCommand,4,0,
                      (struct sockaddr *)&toLink,
                      sizeof(struct sockaddr_in));

               /* request linked nodes */
               queryCommand[0] = 0x04;
               queryCommand[1] = 0xc0;
               queryCommand[2] = 0x05;
               queryCommand[3] = 0x00;
               sendto(g2_sock,(char *)queryCommand,4,0,
                      (struct sockaddr *)&toLink,
                      sizeof(struct sockaddr_in));

               /* request connected users */
               queryCommand[0] = 0x04;
               queryCommand[1] = 0xc0;
               queryCommand[2] = 0x06;
               queryCommand[3] = 0x00;
               sendto(g2_sock,(char *)queryCommand,4,0,
                      (struct sockaddr *)&toLink,
                      sizeof(struct sockaddr_in));

               /* request last-heard */
               queryCommand[0] = 0x04;
               queryCommand[1] = 0xc0;
               queryCommand[2] = 0x07;
               queryCommand[3] = 0x00;
               sendto(g2_sock,(char *)queryCommand,4,0,
                     (struct sockaddr *)&toLink,
                     sizeof(struct sockaddr_in));
            }
            else
            {
               fprintf(stderr,"Login failed\n");
               break;
            }
         }
         else
         if (recvlen > 8)
         {
            /* version */
            if ((queryCommand[2] == 0x03) &&
                (queryCommand[3] == 0x00))
            {
               memcpy(DXRFD_VERSION, queryCommand + 4, 4);
               DXRFD_VERSION[4] = '\0';
            }
            else
            /* connected users */
            if ((queryCommand[2] == 0x06) &&
                (queryCommand[3] == 0x00))
            {
               ptr = queryCommand + 8;
               while ((ptr - queryCommand) < recvlen)
               {
                  temp_string[0] = *ptr;
                  temp_string[1] = ':';
                  memcpy(temp_string + 2, ptr + 1, 8);
                  temp_string[10] = ':';
                  temp_string[11] = *(ptr + 10);
                  temp_string[12] = '\0';
                  if (!strstr(temp_string + 2, "1NFO"))
                     connected_list.insert(temp_string);
                  ptr += 20;
               }
            }
            else
            /* linked repeaters */
            if ((queryCommand[2] == 0x05) &&
                (queryCommand[3] == 0x01))
            {
               ptr = queryCommand + 8;
               while ((ptr - queryCommand) < recvlen)
               {
                  // get the repeater 
                  memcpy(temp_string, ptr + 1, 8);
                  temp_string[8] = '\0';

                  k = -1;
                  if (*ptr == 'A')
                     k = 0;
                  else
                  if (*ptr == 'B')
                     k = 1;
                  else
                  if (*ptr == 'C')
                     k = 2;
                  else
                  if (*ptr == 'D')
                     k = 3;
                  else
                  if (*ptr == 'E')
                     k = 4;
 
                  if (k >= 0)
                     linked_list[k].insert(temp_string);

                  ptr += 20;
               }
            }
            else
            /* last heard list */
            if ((queryCommand[2] == 0x07) &&
                (queryCommand[3] == 0x00) &&
                (recvlen > 10))
            {
               ptr = queryCommand + 10;
               while ((ptr - queryCommand) < recvlen)
               {
                  memset(temp_string, ' ', sizeof(temp_string));

                  tnow = *(uint32_t *)(ptr + 16);
                  mytm = localtime(&tnow);
                  sprintf(temp_string, "%02d%02d%02d-%02d:%02d:%02d",
                          mytm->tm_year % 100, mytm->tm_mon+1, mytm->tm_mday,
                          mytm->tm_hour,mytm->tm_min,mytm->tm_sec);
                  temp_string[15] = ' ';

                  memcpy(temp_string + 30, ptr + 8, 8);

                  strcpy(temp_string + 40,  (char *)ptr);

                  lh_list.insert(temp_string);
                  ptr += 24;
               }
            }
         }
         FD_CLR (g2_sock,&fdset);
      }
      time(&tnow);
      if ((tnow - init_rq) > 5)
      {
         fprintf(stderr, "timeout... is dxrfd running?\n");
         keep_running = false;
      }
   }

   // inizio creazione pagina web

   //  Modificazioni tedesco by oe5stm

   printf("<!DOCTYPE html>\n");
   printf("<html>\n");
   printf("<!-- customized by ik5xmk -->\n");
   printf("<head>\n");
   printf("<title>%s Dashboard Reflector %s</title>\n", argv[3], argv[2]);
   printf("<meta http-equiv=\"refresh\" content=\"%s\">\n", argv[7]);
   printf("<meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\" />\n");
   printf("<link rel=\"stylesheet\" href=\"style_xrf.css\" type=\"text/css\" />\n");
   printf("</head>\n");
   printf("<body>\n");
   printf("<div id=\"container\">\n");
   printf("<header>\n");
   printf("<h2>%s Reflector System %s</h2>\n", argv[3], argv[2]);
   printf("<h3>Dashboard %s | Software dxrfd ver.%s</h3>\n", VERSION,DXRFD_VERSION);
   printf("</header>\n");

   // informazioni di sistema dentro il div con id info

   time(&tnow);
   date_time = ctime(&tnow);

   if(sysinfo(&sys_info) !=0)
     perror("sysinfo");
   days = sys_info.uptime / 86400;
   hours = (sys_info.uptime / 3600) - (days * 24);
   mins = (sys_info.uptime / 60) - (days * 1440) - (hours * 60);

   if ( strcmp(argv[5], "YES") == 0 ) // e' richiesta la visualizzazione delle informazioni di sistema
   {
     printf("<div id=\"info\">\n");
     printf("Status at: %s %s | \n", date_time, (tzname[0] == NULL)?" ":tzname[0]);
     printf("Server Uptime: %i day%s, %i hour%s and %i minute%s</td>\n", days, days==1?"":"s", hours, hours==1?"":"s",  mins, mins==1?"":"s");
     printf("<br>Diese Seite wird alle %s Sekunden aktualisiert.", argv[7]);
     printf("<br><span id=\"contacts\">%s</span>\n", argv[8]);
     printf("</div>\n");
   } else
   {
	// codice in alternativa alla visualizzazione delle info
   }

   // prima tabella relativa ai gateways porta 30001

   printf("<div id=\"num1\">\n");
   printf("<h3 id=\"tabletitle\">Verbindungen Reflector/Repeater/UP4DAR</h3>\n");
   printf("<table class=\"gateways\">\n");
   printf("<tr>\n");
   printf("<th>#</th><th>Modul (A)</th><th>Modul (B)</th><th>Modul (C)</th><th>Modul (D)</th><th>Echo Test (E)</th>\n");
   printf("</tr>\n");

   max_index = linked_list[0].size();
   if (max_index < linked_list[1].size())
      max_index = linked_list[1].size();
   if (max_index < linked_list[2].size())
      max_index = linked_list[2].size();
   if (max_index < linked_list[3].size())
      max_index = linked_list[3].size();
   if (max_index < linked_list[4].size())
      max_index = linked_list[4].size();

   linked_pos[0] = linked_list[0].begin();
   linked_pos[1] = linked_list[1].begin();
   linked_pos[2] = linked_list[2].begin();
   linked_pos[3] = linked_list[3].begin();
   linked_pos[4] = linked_list[4].begin();

   if (max_index > 0)
   {
      for (i = 0; i < max_index; i++) //max_index numero di connessioni presenti sulla 30001
      {
         if (linked_pos[0] != linked_list[0].end())
            strcpy(rptr_x[0], linked_pos[0]->c_str()); 
         else
            strcpy(rptr_x[0], "        ");
 
         if (linked_pos[1] != linked_list[1].end())
            strcpy(rptr_x[1], linked_pos[1]->c_str()); 
         else
            strcpy(rptr_x[1], "        ");

         if (linked_pos[2] != linked_list[2].end())
            strcpy(rptr_x[2], linked_pos[2]->c_str());
         else
            strcpy(rptr_x[2], "        ");

         if (linked_pos[3] != linked_list[3].end())
            strcpy(rptr_x[3], linked_pos[3]->c_str());
         else
            strcpy(rptr_x[3], "        ");

         if (linked_pos[4] != linked_list[4].end())
            strcpy(rptr_x[4], linked_pos[4]->c_str());
         else
            strcpy(rptr_x[4], "        ");


         printf("<tr>\n");
         printf("<td><span id=\"rownum\">%d</span></td>\n", i+1); 

	 // MODULO A
         numriga=0;
         if (( memcmp(rptr_x[0],"XRF",3) != 0 ) && ( memcmp(rptr_x[0],"REF",3) != 0 )) // confronto negativo, non e' un reflector
           printf("<td><a href=\"http://www.aprs.fi/%.6s\">%s</a></td>\n", rptr_x[0], rptr_x[0]);
         else
	{
           for (nn = 1; nn < NRIGHEURL; nn++) //ciclo per tutte le righe lette del file url partendo dalla 1 (la 0 e' commento)
           {
		strncpy(mystr2, url_xrf[nn],6); //copia primi 6 caratteri da riga nn del file url in mystr2
		if (strncmp(mystr2,rptr_x[0],6) == 0)  //confronta con rptr_x[0] e se positivo prepara l'url
		{
			for (nnn=7; nnn <100; nnn++)
			{
				mystr[nnn-7] = url_xrf[nn][nnn];
			}
			numriga=1; //trovata corrispondenza
		}
           }
	   if (numriga == 1)
	   {  
	   	printf("<td><span id=\"refl\">%s<a href=\"%s\"><img src=\"xrf_url_img\" class=\"media\" /></a></span></td>\n", rptr_x[0], mystr);
	   } else
	   {
	        printf("<td><span id=\"refl2\">%s</span></td>\n", rptr_x[0]);
	   }
	}

	// MODULO B
	numriga=0;
         if (( memcmp(rptr_x[1],"XRF",3) != 0 ) && ( memcmp(rptr_x[1],"REF",3) != 0 ))
           printf("<td><a href=\"http://www.aprs.fi/%.6s\">%s</a></td>\n", rptr_x[1], rptr_x[1]);
         else
        {
           for (nn = 1; nn < NRIGHEURL; nn++) 
           {
                strncpy(mystr2, url_xrf[nn],6); 
                if (strncmp(mystr2,rptr_x[1],6) == 0)
                {
                        for (nnn=7; nnn <100; nnn++)
                        {
                                mystr[nnn-7] = url_xrf[nn][nnn];
                        }
                        numriga=1; 
                }
           }
           if (numriga == 1)
           {
                printf("<td><span id=\"refl\">%s<a href=\"%s\"><img src=\"xrf_url_img\" class=\"media\" /></a></span></td>\n", rptr_x[1], mystr);
           } else
           {
                printf("<td><span id=\"refl2\">%s</span></td>\n", rptr_x[1]);
           }
        }

	// MODULO C
	numriga=0;
         if (( memcmp(rptr_x[2],"XRF",3) != 0 ) && ( memcmp(rptr_x[2],"REF",3) != 0 ))
           printf("<td><a href=\"http://www.aprs.fi/%.6s\">%s</a></td>\n", rptr_x[2], rptr_x[2]);
         else
        {
           for (nn = 1; nn < NRIGHEURL; nn++)
           {
                strncpy(mystr2, url_xrf[nn],6);
                if (strncmp(mystr2,rptr_x[2],6) == 0)
                {
                        for (nnn=7; nnn <100; nnn++)
                        {
                                mystr[nnn-7] = url_xrf[nn][nnn];
                        }
                        numriga=1;
                }
           }
           if (numriga == 1)
           {
                printf("<td><span id=\"refl\">%s<a href=\"%s\"><img src=\"xrf_url_img\" class=\"media\" /></a></span></td>\n", rptr_x[2], mystr);
           } else
           {
                printf("<td><span id=\"refl2\">%s</span></td>\n", rptr_x[2]);
           }
        }

	// MODULO D
	numriga=0;
         if (( memcmp(rptr_x[3],"XRF",3) != 0 ) && ( memcmp(rptr_x[3],"REF",3) != 0 ))
           printf("<td><a href=\"http://www.aprs.fi/%.6s\">%s</a></td>\n", rptr_x[3], rptr_x[3]);
         else
        {
           for (nn = 1; nn < NRIGHEURL; nn++)
           {
                strncpy(mystr2, url_xrf[nn],6);
                if (strncmp(mystr2,rptr_x[3],6) == 0)
                {
                        for (nnn=7; nnn <100; nnn++)
                        {
                                mystr[nnn-7] = url_xrf[nn][nnn];
                        }
                        numriga=1;
                }
           }
           if (numriga == 1)
           {
                printf("<td><span id=\"refl\">%s<a href=\"%s\"><img src=\"xrf_url_img\" class=\"media\" /></a></span></td>\n", rptr_x[3], mystr);
           } else
           {
                printf("<td><span id=\"refl2\">%s</span></td>\n", rptr_x[3]);
           }
        }

	// MODULO E
	numriga=0;
         if (( memcmp(rptr_x[4],"XRF",3) != 0 ) && ( memcmp(rptr_x[4],"REF",3) != 0 ))
           printf("<td><a href=\"http://www.aprs.fi/%.6s\">%s</a></td>\n", rptr_x[4], rptr_x[4]);
         else
        {
           for (nn = 1; nn < NRIGHEURL; nn++)
           {
                strncpy(mystr2, url_xrf[nn],6);
                if (strncmp(mystr2,rptr_x[4],6) == 0)
                {
                        for (nnn=7; nnn <100; nnn++)
                        {
                                mystr[nnn-7] = url_xrf[nn][nnn];
                        }
                        numriga=1;
                }
           }
           if (numriga == 1)
           {
                printf("<td><span id=\"refl\">%s<a href=\"%s\"><img src=\"xrf_url_img\" class=\"media\" /></a></span></td>\n", rptr_x[4], mystr);
           } else
           {
                printf("<td><span id=\"refl2\">%s</span></td>\n", rptr_x[4]);
           }
        }

         printf("</tr>\n");

         if (linked_pos[0] != linked_list[0].end())
            linked_pos[0] ++;
         if (linked_pos[1] != linked_list[1].end())
            linked_pos[1] ++;
         if (linked_pos[2] != linked_list[2].end())
            linked_pos[2] ++;
         if (linked_pos[3] != linked_list[3].end())
            linked_pos[3] ++;
         if (linked_pos[4] != linked_list[4].end())
            linked_pos[4] ++;
      }
   }

   printf("</table>\n");
   printf("</div>\n"); // div num1

   // seconda tabella relativa agli hotspot

   printf("<div id=\"num2\">\n");
   printf("<h3 id=\"tabletitle\">Verbindungen Hotspot</h3>\n");
   printf("<table class=\"clients\">\n");
   printf("<tr>\n");
   printf("<th>#</th><th>Rufzeichen</th><th>Status</th><th>Typ</th>\n");
   printf("</tr>\n");

   i = 1;
   for (connected_pos = connected_list.begin(); connected_pos!= connected_list.end(); connected_pos++)
   {
     printf("<tr>\n");

     printf("<td><span id=\"rownum\">%d</span></td>\n", i);
     printf("<td><a href=\"http://query.ke5bms.com/index.php?callsign=%.6s\">%.8s</a></td>\n", connected_pos->c_str()+2,  connected_pos->c_str() + 2);
     if ((connected_pos->c_str())[0] == ' ')
        printf("<td>Hoert zu</td>\n");
     else
	printf("<td>Sendet in<img src=\"arrow_dx.png\" class=\"media2\" /><span id=\"cnxmod\">%c</span></td>\n", (connected_pos->c_str())[0]);

     if ((connected_pos->c_str())[11] == 'H')
        printf("<td>%s</td>\n", "DVMega/Babystar");
     else
     if ((connected_pos->c_str())[11] == 'A')
        printf("<td>%s</td>\n", "DVAP");
     else
     if ((connected_pos->c_str())[11] == 'X')
        printf("<td>%s</td>\n", "DV Dongle");
     else
     if ((connected_pos->c_str())[11] == 'D')
        printf("<td>%s</td>\n", "Thumb DV");
     else

        printf("<td>%c</td>\n", connected_pos->c_str()[11]); // non conosco il tipo, visualizza la lettera 

     printf(" </tr>\n");
     i ++;
   }
   printf("</table>\n");
   printf("</div>\n"); // div num2

   // tabella dei nominativi ultimi ascoltati
   
   printf("<div id=\"num3\">\n");
   printf("<h3 id=\"tabletitle\">Zuletzt gehoerte Rufzeichen</h3>\n");
   printf("<table class=\"calls\">\n");
   printf("<tr>\n");
   printf("<th>#</th><th>Rufzeichen</th><th>gekommen von</th><th>Tag  um Uhrzeit</th>\n");
   printf("</tr>\n");

   i = 1; 
   for (r_lh_pos = lh_list.rbegin(); r_lh_pos != lh_list.rend(); r_lh_pos++)
   {
     memset(temp_user, ' ', 9); temp_user[8] = '\0';
     strncpy(temp_user, r_lh_pos->c_str() + 40, 8);
     temp_user_p = strchr(temp_user, ' ');
     if (temp_user_p)
     {
        *temp_user_p = '\0';

     printf("<tr>\n");
     printf("<td><span id=\"rownum\">%d</span></td>\n", i);
     printf("<td><a href=\"http://www.qrz.com/db/%.8s\">%.8s</a></td>\n", temp_user, r_lh_pos->c_str() + 40);
     printf("<td><span id=\"cnxmod\">%c</span><img src=\"arrow_sx.png\" class=\"media2\">%.6s %c</td>", *(r_lh_pos->c_str() + 37),  r_lh_pos->c_str() + 30, *(r_lh_pos->c_str() + 36));

	 char iso_date[12];
	 iso_date[0] = '2';
	 iso_date[1] = '0';
	 iso_date[2] = *(r_lh_pos->c_str() + 0);
	 iso_date[3] = *(r_lh_pos->c_str() + 1);
	 iso_date[4] = iso_date[7] = '-';
	 iso_date[5] = *(r_lh_pos->c_str() + 2);
	 iso_date[6] = *(r_lh_pos->c_str() + 3);
	 iso_date[8] = *(r_lh_pos->c_str() + 4);
	 iso_date[9] = *(r_lh_pos->c_str() + 5);
	 iso_date[10] = '\0';

	 printf("<td>%s %.17s</td>\n", iso_date, r_lh_pos->c_str()+7);

     i ++;
     printf("</tr>\n");
    }
/* 
    if (i > 20) // modifica per chiudere la tabella a 20 nominativi
    {
	break;
    }
*/
   }

   printf("</table>\n");
   printf("</div>\n");  // div num3

   printf("</div>\n"); // div container e chiusura pagina
   printf("</body>\n");
   printf("</html>\n");

   if (g2_sock != -1)
   {
      fprintf(stderr,"\nRequesting disconnect...\n");
      queryCommand[0] = 5;
      queryCommand[1] = 0;
      queryCommand[2] = 24;
      queryCommand[3] = 0;
      queryCommand[4] = 0;
      sendto(g2_sock,(char *)queryCommand,5,0,
          (struct sockaddr *)&toLink,
          sizeof(struct sockaddr_in));
   }
 
   srv_close();

   return 0;
}
