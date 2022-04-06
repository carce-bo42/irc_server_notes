**Disclaimer**: La siguiente información aplica para IPv4, y algunas cosas también para IPv6.
Pero yo no usaría estas notas para hacer nada en IPv6, si acaso referenciar
el [documento](http://beej.us/guide/bgnet/html/) del que salen estas notas.
Cualquier suggestion es bienvenido, recordermos: escribo esto para tener un sitio
al que venir cuando no tenga ni idea de qué hace mi servidor irc. También
está en castellano porque para hacerlo en inglés ya estan las notas que referencio.

                    #  I N T R O D U C C I O N 

##STRUCTS:

Un socket es simplemente un file descriptor. Tiene un int asociado como
cualquier otro fd, pero tiene la particularidad de que para
escribir/leer a/de él, es muy recomendable usar send/recv que no
write/read. Se entenderá más adelante por qué.

La siguiente estructura se usa para tener a mano las socket address 
structs a la hora de usarlas secuentemente.
```
--- STRUCT ADDRINFO -----
struct addrinfo {
    int              ai_flags;     // AI_PASSIVE, AI_CANONNAME, etc.
    int              ai_family;    // AF_INET, AF_INET6, AF_UNSPEC
    int              ai_socktype;  // SOCK_STREAM, SOCK_DGRAM
    int              ai_protocol;  // use 0 for "any"
    size_t           ai_addrlen;   // size of ai_addr in bytes
    struct sockaddr *ai_addr;      // struct sockaddr_in or _in6
    char            *ai_canonname; // full canonical hostname

    struct addrinfo *ai_next;      // linked list, next node
};

    ---- STRUCT SOCKADDR -----
    
    struct sockaddr {
        unsigned short    sa_family;    // address family, AF_xxx
        char              sa_data[14];  // 14 bytes of protocol address
    };
    ---------------------------

    Y esta struct sockaddr puede usarse como struct sockaddr_in y viceversa :

    ---- STRUCT SOCKADDR IN -----

    (sin stands for socket internet).

    // (IPv4 only--see struct sockaddr_in6 for IPv6)
    struct sockaddr_in {
        short int          sin_family;  // Address family, AF_INET
        unsigned short int sin_port;    // Port number
        struct in_addr     sin_addr;    // Internet address
        unsigned char      sin_zero[8]; // Same size as struct sockaddr
    };

    sin_zero es para igualar el tamaño de esta estructura al de 
    struct sockaddr, así que debería hacerse un memset a ceros de sus 8 bytes.
    -----------------------------

    ----- STRUCT IN_ADDR ---------
    struct in_addr es, por razones históricas:

    struct in_addr {
        uint32_t s_addr; // that's a 32-bit int (4 bytes)
    };
    ------------------------------

------------------- 
```

El procedimiento a seguir es rellenar addrinfo con lo que necesitemos, y
después llamar a getaddrinfo():

int getaddrinfo(const char *restrict node, const char *restrict service,
                const struct addrinfo *restrict hints,
                struct addrinfo **restrict res);

node: "www.example.com" o dirección ip. Aquí se le debería poner el
hostname, obtenido a través de gethostname().
service: el número de puerto en el que queremos el socket. Literalmente
enchufar "6667" y la función se encarga del resto.

Descripción: Aloja un puntero a una nueva struct addrinfo con un node por
direccion válida del host que le pasemos, en el puerto que le pasemos.

Al curioso:
 - por qué struct addrinfo es una lista ???? toma link :
    https://stackoverflow.com/questions/55943056/
 - No está de más decir que man getaddrinfo tiene literalmente un servidor
 udp de ejemplo en código fuente.

Modo de uso en detalle en prueba_addr_info.c.


Se continúa obetniendo el nombre del host con gethostname, y posteriormente
rellenando la estructura siguiente llamando a gethostbyname con el nombre
de host obtenido:
```
---- STRUCT HOSTENT -----

struct hostent {
    char  *h_name;            /* official name of host */
    char **h_aliases;         /* alias list */
    int    h_addrtype;        /* host address type */
    int    h_length;          /* length of address */
    char **h_addr_list;       /* list of addresses */
# define h_addr h_addr_list[0]
};
```
`h_addr_list[0]` debería contener el valor de la IP que esté usando el host.
Maravilloso sería que h_length fuese 4. Si es 6, decimos que no trabajamos 
con ipv6 y listos. Pero este tipo de errores no deberían ocurrir.
Podría haber más de una IP válida (multihost), pero nos da igual aquí.
En todo caso, iterar h_addr_list hasta dar con un NULL o con una IPv4
que nos sirva para bindear (ya veremos qué significa esto).

las entradas de h_addr_list contendrán cada una una IP del host.
En mi caso, me da la de loopback, 127.0.0.1, lo que tiene mucho 
sentido porque estoy en una biblioteca pública y me jodo.

COSAS UTILES :
```
struct sockaddr_in sa; // IPv4
inet_pton(AF_INET, "10.12.110.57", &(sa.sin_addr)); // IPv4
```

escribirá la ip en sa.sin_addr en ipv4 sin que tengamos que matarnos.
la alternativa old school es llamar a
inet_aton(const char* __ip, struct in_addr inp);
La única razón por la que está obsoleta es porque no acepta AF_INET6.

Fin de la introducción.

Prepare to launch !

- Con getaddrinfo, tenemos todo lo que necesitamos, dado un hostname
y un puerto. Si no tenemos un hostname, a veces el firewall nos mandará
a la mierda (es el caso de la biblioteca, que cuando le pongo "PC-CARCE"
de argumento node a getaddrinfo me devuelve ip 127.0.1.1, y cuando le
pongo NULL me pone 0.0.0.0). Detalles en prueba_addr_info.c.


##SOCKET CALL

int socket(int domain, int type, int protocol);

El dominio es para decir qué tipo de IP se utilizará. Para IPv4, PF_INET y
para IPv6 PF_INET6. Por qué PF en vez de AF como sucede con la sin_family 
en struct sockaddr_in ? Pues como todas las cosas que son raras, por razones
históricas. El resumen es que se quiso en un principio tener varios 
Protocol Families (PF) por cada Adress Family (AF), pero nunca se consiguió
y actualmente el mapeo AF PF es uno a uno.

Llamas a socket, le dices si PF_INET o PF_INET6, y luego qué ? El tipo. 
SOCK_DGRAM para UDP y SOCK_STREAM para TCP. El protocolo ? En mi caso, que
estoy leyendo una guía para entender todo esto, no debería especificarlo
porque se me escapa muchísimo (pongo 0 y listos). Si se setea a 0, la propia
llamada determina el protocolo dado el tipo.

Hasta ahora, lo que sabemos es que dado un host (ya sea en forma de nombre, de IP
o de dominio) y un puerto:

{

#define CATASTROFE 1
int status;
struct addrinfo hints;
struct addrinfo *servinfo;

memset(&hints, 0, sizeof hints);
hints.ai_family = AF_INET;
hints.ai_socktype = SOCK_STREAM;

// ej.
char* host = "1.1.1.1"; // o "www.example.com" o "PC-PEPITO"
char* port = "9999";

// creamos la struct addrinfo a partir de hints 
if (getaddrinfo(host, port, &hints, &servinfo) != 0)
    return CATASTROFE;

int socketfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
if (socketfd == -1)
    return CATASTROFE;

}

Con el socketfd se tiene por tanto el fd necesario para proceder
con las siguientes llamadas al sistema.

Es interesante ver que la llamada a socket sólo tiene, 
parece ser, 2 parámetros. Uno que da el tipo de socket, y otro otra
IP (de verdad, suda del protocolo).
Nada más. Sobre ese fd el sistema no sabe absolutamente nada más.

BIND CALL

Aquí es donde se le dice qué puerto es el que va a tener asociado ese
socket. Que qué significa esto ? Ni idea, pero sí ha de sonar que
las IP's y los puertos van bastante de la mano cuando se trata de servidores,
y como bien he mencionado antes, al socket de momento no se le ha dicho
nada sobre el puerto.



