**Disclaimer**: La siguiente información aplica para IPv4, y algunas cosas también para IPv6.
Pero yo no usaría estas notas para hacer nada en IPv6, si acaso referenciar
el [documento](http://beej.us/guide/bgnet/html/) del que salen estas notas.
Cualquier suggestion es bienvenida, recordermos: escribo esto para tener un sitio
al que venir cuando no tenga ni idea de qué hace mi servidor irc. También
está en castellano porque para hacerlo en inglés ya estan las notas que referencio.

# I N T R O D U C C I O N 

## STRUCTS

Un socket es simplemente un file descriptor.  Por el simple hecho de ser llamado desde tu ordenador, tiene acceso a las IP y los puertos **locales** que tengamos. Tiene un int asociado como cualquier otro fd, pero tiene la particularidad de que para escribir/leer a/de él, es muy recomendable usar send/recv que no write/read. Se entenderá más adelante por qué.

La siguiente estructura se usa para tener a mano las socket address structs a la hora de usarlas secuentemente. Se llama struct addrinfo, struct addrinfo lector, lector struct addrinfo.
```

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
```

Donde `struct sockaddr` es:

```
    struct sockaddr {
        unsigned short    sa_family;    // address family, AF_xxx
        char              sa_data[14];  // 14 bytes of protocol address
    };
```

Y esta `struct sockaddr` puede usarse como `struct sockaddr_in` y viceversa ('sin' viene de socket internet). 

```
    // (IPv4 only--see struct sockaddr_in6 for IPv6)
    struct sockaddr_in {
        short int          sin_family;  // Address family, AF_INET
        unsigned short int sin_port;    // Port number
        struct in_addr     sin_addr;    // Internet address
        unsigned char      sin_zero[8]; // Same size as struct sockaddr
    };
```

Puede verse que struct sockaddr_in tiene todo mejor localizado, y por tanto es la que se recomienda utilizar. `sin_zero[8]` es para igualar el tamaño de esta estructura al de `struct sockaddr`, así que debería hacerse un memset a ceros de sus 8 bytes. Esto que parece tan raro es coherente con el "usarse como" que he dicho antes. Con ello, me refiero a que este código es perfectamente válido, y es en parte gracias a este `sin_zero[8]`:

```
    struct addrinfo *servinfo;
    // ... 
    struct sockaddr_in *addr_in = (struct sockaddr_in *)servinfo->ai_addr;
    printf("addr_in : [%s]\n", inet_ntoa(addr_in->sin_addr));
```

Volviendo a los elementos en `struct sockaddr_in`, que se sepa que `struct addr_in`es simplemente un 4-byte int. Por qué? Razones históricas.

```
    struct in_addr {
        uint32_t s_addr; // that's a 32-bit int (4 bytes)
    };
```

El procedimiento a seguir es rellenar una `struct addrinfo` con lo que necesitemos, y después llamar a getaddrinfo() para que nos complete otra estructura del mismo tipo pero con más información.

```
int getaddrinfo(const char *restrict node, const char *restrict service,
                const struct addrinfo *restrict hints,
                struct addrinfo **restrict res);
```

*node*: "www.example.com" o dirección ip. Aquí se le debería poner el hostname, obtenido a través de gethostname().

*service*: el número de puerto en el que queremos el socket. Literalmente enchufar "6667" y la función se encarga del resto. 

Descripción: Aloja un puntero a una nueva struct addrinfo con un node por direccion válida del host que le pasemos, en el puerto que le pasemos. Devuelve 0 si todo va bien. 

Al curioso:
 - por qué struct addrinfo es una lista ???? toma link :
    https://stackoverflow.com/questions/55943056/
 - No está de más decir que man getaddrinfo tiene literalmente un servidor
 udp de ejemplo en código fuente.

Modo de uso en detalle en prueba_addr_info.c.

Se continúa obetniendo el nombre del host con gethostname, y posteriormente rellenando la estructura siguiente llamando a gethostbyname con el nombre de host obtenido:

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
`h_addr_list[0]` debería contener el valor de la IP que esté usando el host. Maravilloso sería que h_length fuese 4. Si es 6, decimos que no trabajamos con ipv6 y listos. Pero este tipo de errores no deberían ocurrir. Podría haber más de una IP válida (multihost), pero nos da igual aquí. En todo caso, iterar h_addr_list hasta dar con un NULL o con una IPv4
que nos sirva para bindear (ya veremos qué significa esto).

Las entradas de h_addr_list contendrán cada una una IP del host. En mi caso, me da la de loopback, 127.0.0.1, lo que tiene mucho sentido porque estoy en una biblioteca pública y me jodo.

## COSAS UTILES :

```
struct sockaddr_in sa; // IPv4
inet_pton(AF_INET, "10.12.110.57", &(sa.sin_addr)); // IPv4
```

escribirá la IP **local** en sa.sin_addr en ipv4 sin que tengamos que matarnos. Recordemos que struct sockaddr_in por mucha struct que sea es un 4-byte int. 

La alternativa old school es llamar a 
```
 inet_aton(const char* __ip, struct in_addr inp);
```

La única razón por la que está obsoleta es porque no acepta AF_INET6.

# FUNCTIONES SOCKET RELATED 

Con `getaddrinfo`, tenemos todo lo que necesitamos, dado un hostname y un puerto. Si no tenemos un hostname, a veces el firewall nos mandará a la mierda (es el caso de la biblioteca, que cuando le pongo "PC-CARCE" de argumento node a getaddrinfo me devuelve la IP 127.0.1.1, y cuando le pongo NULL me pone 0.0.0.0). Detalles en prueba_addr_info.c.

## SOCKET 

```
#include <sys/types.h>
#include <sys/socket.h>
    
int socket(int domain, int type, int protocol);
```

El dominio es para decir qué tipo de IP **local** se utilizará. Para IPv4, `PF_INET` y para IPv6 `PF_INET6`. Por qué PF en vez de AF como sucede con la sin_family en `struct sockaddr_in` ? Razones históricas. El resumen es que se quiso en un principio tener varios Protocol Families (PF) por cada Adress Family (AF), pero nunca se consiguió y actualmente el mapeo AF PF es uno a uno.

Llamas a socket, le dices si `PF_INET` o `PF_INET6`, y luego qué ? El tipo. `SOCK_DGRAM` para UDP y `SOCK_STREAM` para TCP. El protocolo ? En mi caso, que estoy leyendo una guía para entender todo esto, no debería especificarlo
porque se me escapa muchísimo. Lo bueno es que si se procede con `getaddrinfo` hay una entrada en la estructura que devuelve para este parámetro, que será coherente con el tipo `sin_family`que le hayamos indicado. 

Hasta ahora, lo que sabemos es que dado un host (ya sea en forma de nombre, de IP o de dominio) y un puerto, este código consigo tener un socket asociado a dicha IP, del tipo que le digamos:

```
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
```

Con el socketfd se tiene por tanto el fd necesario para proceder con las siguientes llamadas al sistema.

Es interesante ver que la llamada a socket sólo tiene, parece ser, 2 parámetros. Uno que da el tipo de socket, y otro otra
IP (de verdad, suda del protocolo). Nada más. Sobre ese fd el sistema no sabe absolutamente nada más.

## BIND 

Aquí es donde se le dice qué puerto **local** es el que va a tener asociado ese socket. Que qué significa esto ? Ni idea, pero como bien he mencionado antes, al socket de momento no se le ha dicho nada sobre el puerto. Se utiliza, por tanto, la siguiente llamada a sistema para ligar ese socket que ya tiene la IP que le hemos dado al puerto que le demos:

```
    #include <sys/types.h>
    #include <sys/socket.h>
    
    int bind(int sockfd, struct sockaddr *my_addr, int addrlen);
```

Donde `sockfd` es el fd del socket obtenido anteriormente, y `my_addr`  y `addr_len` deberían ser las entrada `ai_addr` y `ai_addrlen`, respectivamente, de nuestra estructura devuelta por `getaddrinfo`.  De esta forma, socket y bind se llaman con la misma IP, y con la última llamada se añade el puerto. 

Importante saber que puertos por debajo del 1024 están o suelen estar reservados para protocolos específicos. [Aquí](https://en.wikipedia.org/wiki/List_of_TCP_and_UDP_port_numbers) hay una lista con toda la información necesaria sobre el tema. 

Devuelve -1 cuando falla.

A veces `bind()` falla sobre un socket que haya sido utilizado hace poco. Cuando se resetea un servidor por ejemplo. El error suele ser "address already in use". Esto normalmente al minuto se pasa. Pero para evitar que pase, existe una función que, a parte de esto, permite setear opciones en el socket:

```
int setsockopt(int socket, int level, int option_name,
       const void *option_value, socklen_t option_len);
```

Un montón de cosas se me escapan sobre cómo funciona la función o qué cosas consigue hacer. Pero el ejemplo que se da en el documento que estoy leyendo es el siguiente y sirve para evitar el error anteriormente mencionado:

```
int yes=1;
//char yes='1'; // Solaris people use this

// lose the pesky "Address already in use" error message
if (setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof yes) == -1) {
    perror("setsockopt");
    exit(1);
} 
```

Importante decir que `bind()` no es importante a menos que queramos especificar el puerto **local** con el que trabaje el socket. Si lo que queremos es conectarnos a un servidor, `connect()` se encargará de bindear por debajo a un puerto no usado de nuestra máquina para hablar con dicho servidor. 

## CONNECT 

Hasta ahora ha sido todo muy local. Por eso lo he puesto en **bold** siempre que he podido. Es ahora y sólo ahora cuando se pasa a hablar de conectarse a IP's y puertos que no sean locales.

```
    #include <sys/types.h>
    #include <sys/socket.h>
    
    int connect(int sockfd, struct sockaddr *serv_addr, int addrlen); 
```

El prototipo es el mismo que `bind()`, sólo que lo que hace es, de forma bastante literal, es *conectar* un socket (definido localmente) a una IP y puerto especificados dentro de `struct sockaddr` cualquieras. Que funcione o no dependerá de que esa IP y puerto contenidos en esa `struct sockaddr` sean de un servidor que está escuchando o `listen()` en ese puerto e IP.  

Devuelve 0 en caso de todo bien. Se explicará más adelante qué cosas podremos hacer con el socket una vez hayamos conseguido conectarlo a un servidor.

## LISTEN

Esta función es en el caso de ser nosotros un servidor. Una vez creado el socket, después de la llamada a `bind()`, tenemos todo lo necesario para llamar a `listen()`. Esta función sabrá  que ese socket tiene esa IP **local** con ese puerto **local** bindeado, y empezará a detectar conexiones entrantes a esa (IP:puerto). Y esas conexiones que le lleguen vendrán de un cliente que ha creado su propio socket con sus mierdas y se está `connect()` a nuestra IP:puerto donde le hemos dicho que estaremos escuchando. 

```
    int listen(int sockfd, int backlog); 
```

El sockfd es el socket de nuestro servidor que queremos poner a escuchar. y backlog es el *número de conexiones permitidas en la cola de entrada* (normalmente backlog ~ 10). La cola de entrada es el "limbo" donde se quedan las conexiones mientras no las hayamos `accept()`ado, porque está bien eso de escuchar un puerto, pero también está bien que el servidor se guarde el derecho de admisión, o en este caso, que se lo delegue a una llamada que sólo el servidor puede hacer dada una conexión entrante. 

`listen()` devuelve -1 en caso de error.

## ACCEPT

Surjen muchas dudas cuando se invoca `listen()`. La primera es qué pasa en el servidor cuando alguien intenta conectarse después de haber llamado a `listen()`. Porque de ninguna manera ha quedado respondida esta duda. Pero se responde rápido: no pasa nada si el servidor no llama a `accept()`. O bueno sí pasa. Entran conexiones pero se quedan en ese "limbo" anteriormente definido como *cola de entrada* por siempre.

El prototipo es el siguiente:

```
    int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen); 
```

Se le introduce el sockfd al que le hayamos previamente hecho `listen()`, y se le pasa la dirección de memoria de una `struct sockaddr` para que la rellene, así como el puntero a la longitud de la estructura que se le pase. Un ejemplo de uso ayudará a digerir esta parte sin duda:

```
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#define MYPORT "3490"  // the port users will be connecting to
#define BACKLOG 10     // how many pending connections queue will hold

int main(void)
{
    struct sockaddr_storage their_addr;
    socklen_t addr_size;
    struct addrinfo hints, *res;
    int sockfd, new_fd;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;  // use IPv4 or IPv6, whichever
    hints.ai_socktype = SOCK_STREAM;

    getaddrinfo(NULL, MYPORT, &hints, &res);

    // make a socket, bind it, and listen on it:
    sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    bind(sockfd, res->ai_addr, res->ai_addrlen);
    listen(sockfd, BACKLOG);

    // now accept an incoming connection:
    addr_size = sizeof(their_addr);
    new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &addr_size);

    // ready to communicate on socket descriptor new_fd!
	return 0;
}
```

Este `new_fd` se convierte en el famoso socket tal y como lo conocemos, en el que las llamadas de `send()`y `recv()`podrán suceder para que cliente y servidor se comuniquen. 

Es importante saber que `accept()` devuelve un fd por **conexión** y no por llamada. El programa se quedará colgado en la llamada a `accept()` hasta que llegue una conexión y entonces sea cuando devuelva el fd y el código siga. Para llevar un servidor, el `accept()`deberá estar en un bucle para gestionar todas las conexiones entrantes. No se menciona qué pasa cuando recibes más conexiones que las que hemos configurado en `listen()`.  Como todo en esta vida, depende. [Aquí](https://stackoverflow.com/questions/37609951/why-i-dont-get-an-error-when-i-connect-more-sockets-than-the-argument-backlog-g) y [aquí](https://stackoverflow.com/questions/10002868/what-value-of-backlog-should-i-use#:~:text=Basically%2C%20what%20the%20listen(),passing%20that%20is%20generally%20safe.) lo que sale en google cuando lo pregunto. 

## SEND / RECV

Una vez se tiene el famoso socketfd respuesta de `accept()`, se pueden invocar las siguientes llamadas. De forma absolutamente coherente, `send()`envía y `recv()` recibe.

```
    int send(int sockfd, const void *msg, int len, int flags);
```

El documento del que saco todo esto me dice que setee flags a 0. Y que si no me quedo tranquilo me lea el man. Me quedo con ponerlo a cero. El caso es que `send()` escribe (y envía)  `len` bytes de `msg` al socketfd que tengamos. Interesante es que por la parte del cliente, le basta con usar el mismo socket que usó para hacer `connect()`. El servidor lee del socketfd respuesta de `accept()`, y el cliente al único socket que tiene.

Para la función `recv()` se tiene:

```
    int recv(int sockfd, void *buf, int len, int flags);
```

Una vez más, flags a 0. Y sino man. Lee un máximo de `len` bytes de sockfd y los escribe en `buf`. Igual que la función `read()`, devuelve el número de bytes leídos. `recv()` puede también devolver 0. Esto sólo sucede cuando el lado remoto ha cerrado la conexión.

Me surjen dudas como qué pasa si llamo a recv sobre un socket en el que el lado remoto no ha escrito nada. Se queda igual que `accept()` cuando no le entran conexiones? Suficentenpor hoy.

Ambas funciones devuelven -1 en caso de error.





 

