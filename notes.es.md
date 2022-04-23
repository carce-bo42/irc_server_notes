



**Disclaimer**: La siguiente información aplica para IPv4, y algunas cosas también para IPv6.
Pero yo no usaría estas notas para hacer nada en IPv6, si acaso referenciar
el [documento](http://beej.us/guide/bgnet/html/) del que salen estas notas. También he leído [este](https://docs.oracle.com/cd/E19620-01/805-4041/6j3r8iu2l/index.html).
Cualquier suggestion es bienvenida, recordermos: escribo esto para tener un sitio
al que venir cuando no tenga ni idea de qué hace mi servidor irc. También
está en castellano porque para hacerlo en inglés ya estan las notas que referencio.

# I N T R O D U C C I O N 

## STRUCTS

Un socket es simplemente un file descriptor.  Por el simple hecho de ser llamado desde tu ordenador, tiene acceso a las IP y los puertos **locales** que tengamos. **Locales** porque es razonable que desde tu ordenador no puedas abrir un socket en una IP que no es 'tuya'. Tiene un int asociado como cualquier otro fd, pero tiene la particularidad de que para escribir/leer a/de él, es muy recomendable usar send/recv que no write/read. Se entenderá más adelante por qué.

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
`h_addr_list[0]` debería contener el valor de la IP que esté usando el host. Maravilloso sería que h_length fuese 4. Si es 6, decimos que no trabajamos con ipv6 y listos. Pero este tipo de errores no deberían ocurrir *****. Podría haber más de una IP válida (multihost), pero nos da igual aquí. En todo caso, iterar h_addr_list hasta dar con un NULL o con una IPv4
que nos sirva para bindear (ya veremos qué significa esto).

***** Me falta un lugar en el man que diga que o la `h_addr_list` es o ipv6 o ipv4. Porque habla de un array con IP's, por tanto, una o más IP's, pero sólo tiene indicado un parámetro `h_length`. Se asume entonces que `h_length` es la misma para todas las entradas de `h_addr_list` ? Como no encuentro una respuesta, lo razonable aquí sería que sí. Aunque nunca se sabe, algunos desarrolladores de librerías parecen tener una adicción a soltar la keyword `Undefined Behaviour` y quedarse tan tranquilos. 

Las entradas de h_addr_list contendrán cada una una IP del host. En mi caso, me da la de loopback, 127.0.0.1, lo que tiene mucho sentido porque estoy en una biblioteca pública y me jodo.

## COSAS UTILES :

```
struct sockaddr_in sa; // IPv4
inet_pton(AF_INET, "10.12.110.57", &(sa.sin_addr)); // IPv4
```

escribirá la IP **local** en sa.sin_addr en ipv4 sin que tengamos que matarnos. Recordemos que struct sockaddr_in por mucha struct que sea es un 4-byte int. 

La alternativa old school es llamar a 
```
 inet_aton(const char* __ip, struct in_addr *inp);
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

Hasta ahora, lo que sabemos es que dado un host (ya sea en forma de nombre, de IP o de dominio) y un puerto, con este código consigo tener un socket asociado a dicha IP, del tipo que le digamos:

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

Aquí es donde se le dice qué puerto **local** es el que va a tener asociado ese socket. Que qué significa esto ? Ni idea, pero como bien he mencionado antes, al socket de momento no se le ha dicho nada sobre el puerto. 

Se utiliza, por tanto, la siguiente llamada a sistema para ligar ese socket que ya tiene la IP que le hemos dado al puerto que le demos:

```
    #include <sys/types.h>
    #include <sys/socket.h>
    
    int bind(int sockfd, struct sockaddr *my_addr, int addrlen);
```

Donde `sockfd` es el fd del socket obtenido anteriormente, y `my_addr`  y `addr_len` deberían ser las entrada `ai_addr` y `ai_addrlen`, respectivamente, de nuestra estructura devuelta por `getaddrinfo`.  De esta forma, socket y bind se llaman con la misma IP, y con la última llamada se añade el puerto. 

Importante saber que los puertos por debajo del 1024 están o suelen estar reservados para protocolos específicos. [Aquí](https://en.wikipedia.org/wiki/List_of_TCP_and_UDP_port_numbers) hay una lista con toda la información necesaria sobre el tema. 

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
if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
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

El prototipo es el mismo que `bind()`, sólo que lo que hace es, de forma bastante literal, *conectar* un `sockfd` (definido localmente) a una IP y puerto especificados dentro de `struct sockaddr` cualquieras. De más está decir que `addrlen` habrá de ser la longitud en bytes de la IP del servidor contenida en `serv_addr`. Que funcione o no dependerá de que esa IP y puerto contenidos en esa `struct sockaddr` sean de un servidor que está escuchando o `listen()` en ese puerto e IP.  

Devuelve 0 en caso de todo bien. Se explicará más adelante qué cosas podremos hacer con el socket una vez hayamos conseguido conectarlo a un servidor.

## LISTEN

Esta función es en el caso de ser nosotros un servidor. Una vez creado el socket, después de la llamada a `bind()`, tenemos todo lo necesario para llamar a `listen()`. Esta función sabrá  que ese socket tiene esa IP **local** con ese puerto **local** bindeado, y empezará a detectar conexiones entrantes a esa (IP:puerto). Y esas conexiones que le lleguen vendrán de un cliente que ha creado su propio socket con su (IP:puerto) **locales** y se está `connect()` a nuestra (IP:puerto) donde le hemos dicho que estaremos escuchando. 

```
    int listen(int sockfd, int backlog); 
```

El sockfd es el socket de nuestro servidor que queremos poner a escuchar. y backlog es el *número de conexiones permitidas en la cola de entrada* (normalmente backlog ~ 10). La cola de entrada es el "limbo" donde se quedan las conexiones mientras no las hayamos `accept()`ado, porque está bien eso de escuchar un puerto, pero también está bien que el servidor se guarde el derecho de admisión, o en este caso, que se lo delegue a una llamada que sólo el servidor puede hacer dada una conexión entrante. 

`listen()` devuelve -1 en caso de error.

## ACCEPT

Surjen muchas dudas cuando se invoca `listen()`. La primera es qué pasa en el servidor cuando alguien intenta conectarse después de haber llamado a `listen()`. Porque de ninguna manera ha quedado respondida esta duda. Pero se responde rápido: no pasa nada si el servidor no llama a `accept()`. O bueno sí pasa. Entran conexiones se quedan en ese "limbo" anteriormente definido como *cola de entrada* por siempre.

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

Al curioso que haya visto que utiliza (el señor del que saco todo esto) una estructura que no he mencionado al principio, primero eres de los míos y segundo toma [link](https://stackoverflow.com/questions/19528820/difference-between-sockaddr-and-sockaddr-storage).

Este `new_fd` se convierte en el famoso socket tal y como lo conocemos, en el que las llamadas de `send()`y `recv()`podrán suceder para que cliente y servidor se comuniquen. 

Es importante saber que `accept()` devuelve un fd por **conexión** y no por llamada. El programa se quedará colgado en la llamada a `accept()` hasta que llegue una conexión y entonces sea cuando devuelva el fd y el código siga. Para llevar un servidor, el `accept()`deberá estar en un bucle para gestionar todas las conexiones entrantes. No se menciona qué pasa cuando recibes más conexiones que las que hemos configurado en `listen()`.  Como todo en esta vida, depende. [Aquí](https://stackoverflow.com/questions/37609951/why-i-dont-get-an-error-when-i-connect-more-sockets-than-the-argument-backlog-g) y [aquí](https://stackoverflow.com/questions/10002868/what-value-of-backlog-should-i-use#:~:text=Basically%2C%20what%20the%20listen(),passing%20that%20is%20generally%20safe.) lo que sale en google cuando lo pregunto. 

## SEND / RECV

Una vez se tiene el famoso socketfd respuesta de `accept()`, se pueden invocar las siguientes llamadas. De forma absolutamente coherente, `send()`envía y `recv()` recibe.

```
    int send(int sockfd, const void *msg, int len, int flags);
```

El documento del que saco todo esto me dice que setee flags a 0. Y que si no me quedo tranquilo me lea el man. Me quedo con ponerlo a cero. El caso es que `send()` escribe (y envía)  `len` bytes de `msg` al socketfd que tengamos. Interesante es que por la parte del cliente, le basta con usar el mismo socket que usó para hacer `connect()`. **El servidor lee/escribe del socketfd respuesta de `accept()`, y el cliente al único socket que tiene.** Importante mencionar que `send()` no tiene por qué devolver el valor indicado en `len`. Si le dices que envíe 1 MB, 100% te mandará a la mierda. Si le pides 1K, a no ser que estés en fin de año a las 00:00, no debería haber ningún problema. Up 2 You queda reenviarlo en caso de que el retorno sea menor que el esperado. 

Para la función `recv()` se tiene:

```
    int recv(int sockfd, void *buf, int len, int flags);
```

Una vez más, flags a 0. Y sino man. Lee un máximo de `len` bytes de sockfd y los escribe en `buf`. Igual que la función `read()`, devuelve el número de bytes leídos. `recv()` puede también devolver 0. Esto sólo sucede cuando el lado remoto ha cerrado la conexión.

Me surjen dudas como qué pasa si llamo a recv sobre un socket en el que el lado remoto no ha escrito nada. Se queda igual que `accept()` cuando no le entran conexiones? **Respuesta**: He leífo a [éste vato](https://stackoverflow.com/questions/59326605/how-to-skip-recv-function-when-no-data-is-sent) y parece ser que en efecto  el llamador se queda colgado hasta que hay datos por leer. Aunque parece ser que es buena praxis no llamar a `recv()` a menos que se tenga la total seguridad de que en el socket hay datos. 

Ambas funciones devuelven -1 en caso de error.

## SENDTO / RECVFROM

Esto no es importante para el IRC pero si no estoy poniendo código para que se copie y se pegue y funcione (literalmente he escrito 0 C++), se entiende que todo este es para aprender y lejos queda el propósito de entregar un proyecto por entregarlo. 

Al protocolo UDP no se le ha de `connect()`. que pasa si a un socket que has llamado con tipo `SOCK_DGRAM` lo conectas? IBM tiene [la respuesta](https://www.ibm.com/docs/en/zos/2.4.0?topic=functions-connect). El enlace anterior es un **must read**, yo lo dejo ahí. Bueno no lo dejo ahí. Es literalmente como se usan los SOCK_DGRAM. Porque recordemos que se usan para transmitir datos de forma rápida y sin garantía de llegada. Por lo tanto *prácticamente siempre* se usan sabiendo a dónde se quiere enviar, lo que TCP no se usa a menos que sea estrictamente necesario conservar la totalidad de la información. Los prototipos para `sendto`y `recvfrom` son los siguientes:

```
int sendto(int sockfd, const void *msg, int len, unsigned int flags,
               const struct sockaddr *to, socklen_t tolen); 
               
int recvfrom(int sockfd, void *buf, int len, unsigned int flags,
                 struct sockaddr *from, int *fromlen);    
```

Puesto que un socket UDP no se conecta y el servidor ni listen ni accept tampoco, ninguno sabe nada ni de a quién enviar ni de quién recibir. Por eso hay que decirlo a quién enviarle en `sendto` y de dónde leer a `recvfrom`. 

## CLOSE / SHUTDOWN

`close()` es la de toda la vida, aplicable a un fd. Cierra por completo un fd. `Shutdown()`, sin embargo, tiene tres formas de interaccionar con el socket que le pasemos. Estas tres formas, se le indican mediante un segundo parámetro en la llamada:

```
   int shutdown(int sockfd, int how); 
```

`how` puede valer 0, 1 o 2. 0 singificará que no reciba más mensajes. 1 que no enviará más mensajes. Y 2 es 0 y 1 con la diferencia de que no cerrará el fd. Acerca de la diferencia entre llamar a shutdown con `how=2` o `close()` hay muchísimo detalle con respecto al uso de uno u otro. No en el archivo que me leo, sino en internet. [Aquí](https://stackoverflow.com/questions/4160347/close-vs-shutdown-socket) hay un individuo en stackoverflow que en su día se preguntó lo mismo, al que le respondieron con puntos y comas.

## FUNCIONES QUE BLOQUEAN

Aquí el documento parece responder esas dudas que me han ido surgiendo sobre algunas funciones en determinadas condiciones. Llama bloqueo a lo que yo he llamado hasta ahora "quedarse colgado". Lo reitero porque creo que es importante tenerlas agrupadas en un mismo bloque de texto. 

 Llamar a `recv()` o `recvfrom()` sobre un socket que no tiene datos, bloquea el programa hasta que los tiene. Llamar a `accept()` cuando hayamos seteado un socket para que escuche (con `listen()`), bloqueará el programa hasta que entre una conexión. 

Esto sucede porque al crear un socket descriptor con `socket()`, el kernel le atribuye esta propiedad. Para que un socket no sea 'blocking', podemos usar la siguiente llamada a `fcntl()` (de hecho esta hasta la dejan usar en el subject).

```
#include <unistd.h>
#include <fcntl.h>

sockfd = socket(PF_INET, SOCK_STREAM, 0);
fcntl(sockfd, F_SETFL, O_NONBLOCK);
```

Haciendo esto,  cuando se llame a alguna función de las anteriormente mencionadas en condiciones de que bloquee, devolverá -1 y escribirá en **errno** `EAGAIN` o `EWOULDBLOCK`. Es recomendable checkear las dos porque que errno valga una u otra depende del sistema. Pero estar haciendo esto es contínuamente hacer llamadas al sistema (asumo que de `accept()`) junto con comprobaciones sobre el errno constantes. Es como programar al asno en Shrek preguntando si han llegado ya a Muy Muy Lejano.

Se introduce para utilizar los recursos del ordenador de forma más eficiente la llamada `poll()`.

## POLL

Trabaja con una estructura llamada `pollfd` que tiene la siguiente pinta:

    struct pollfd {
        int fd;         // the socket descriptor
        short events;   // bitmap of events we're interested in
        short revents;  // when poll() returns, bitmap of events that occurred
    };

El fd es, efectivamente, el socket y events/revents es como decirle qué tendrá que vigilarse respecto a ese socket y cuál de esas cosas que le hemos dicho que vigile (al kernel) se han detectado en el socket (después de la llamada a `poll()`). Tanto events como revents son shorts y se interpretan como flags escritas en 16 bits. 7 de esos 16 bits que componen un short (en una arquitectura moderna de 64-bits) indican una flag distinta. La razón por la que aún así se escoje un short en vez de un char puede deberse al [struct padding](https://stackoverflow.com/questions/4306186/structure-padding-and-packing), o simplemente para tener donde cojer en caso de inventarse nuevos tipos de eventos.  Quizá una combinación de las dos. 

```
#include <poll.h>
int poll(struct pollfd fds[], nfds_t nfds, int timeout);
```

La primera entrada es el array con cada estructura de cada socket, `nfds` es el número de `struct pollfd` que tiene el array `fds[]` y `timeout` es, en milisegundos, el máximo tiempo que queremos que se espere a que un evento suceda. 

Los eventos que nos interesan son los siguientes (copypaste):

| Macro     | Description                                                  |
| --------- | ------------------------------------------------------------ |
| `POLLIN`  | Alert me when data is ready to `recv()` on this socket.      |
| `POLLOUT` | Alert me when I can `send()` data to this socket without blocking. |

`POLLIN`, la que más usaremos y la más fácil de entender. `POLLOUT`, en cambio, es más extraña. Un ejemplo muy bueno sobre la utilización de ambos flags (que sirve para entender `POLLOUT`) es [este link](https://stackoverflow.com/questions/12170037/when-to-use-the-pollout-event-of-the-poll-c-function). Aunque uno puede escribir un servidor IRC sin necesidad de utilizarlo, sin duda.

Cuando llamamos a `poll()` éste devuelve *el número de sockets en `fds[]` en los que se ha dado uno o más de los eventos indicados en cada socket fd*. Esto es, si asigno a `fds[0].events = POLLIN` y `fds[1].events = POLLOUT` y se da que -el fd- 0 recibe datos y el 1 está listo para escribir, poll devolverá 2. Y si sucede que -el fd- 0 no recibe datos y 1 no está listo para escribir, devolverá 0. Se entiende supongo. Que al final solo se usa `POLLIN` y el retorno se resume al número total de sockets que tienen datos para leer ? Sí. Pero me daría por culo decir eso y quedarme tan tranquilo.

Existen tres eventos más que por mucho que no indiquemos en `events` serán procesados, y estos son:

| MACRO            DESCRIPTION                                 |      |
| ------------------------------------------------------------ | ---- |
| `POLLERR`            Error                                   |      |
| `POLLHUP `            El otro extremo ha cerrado la conexión. |      |
| `POLLNVAL`         El fd no está abierto. No cerrar un fd después de recibir este parámetro. |      |

La descripción de estas 3 otras opciones la he encontrado [aquí](https://stackoverflow.com/questions/24791625/how-to-handle-the-linux-socket-revents-pollerr-pollhup-and-pollnval). Y [aquí](https://www.ibm.com/docs/en/zos/2.2.0?topic=functions-poll-monitor-activity-file-descriptors-message-queues) explica bastante bien algunas de las macros (esta es de IBM en vez de SO).

## SELECT

No explicaré qué hace `select` porque por más que busco sólo encuentro que `poll` es más eficiente que `select`. Y es que `select` consigue lo mismo pero teniendo que reescribir todo el array de fd's cada vez que se itera. Mis referencias son [éste artículo](https://daniel.haxx.se/docs/poll-vs-select.html#:~:text=select()%20only%20uses%20(at,more%20over%20to%20kernel%20space)) y [éste vídeo](https://www.youtube.com/watch?v=dEHZb9JsmOU) (el vídeo contiene además ejemplos de código muy útiles).

De todas formas, no está de más decir que en aplicaciones reales ni la una ni la otra se utiliza. Se utilizan librerías con event handlers como por ejemplo [libevent](https://libevent.org/). Hay muchas más, pero no quería dejar esto así como si los servidores de Google estuviesen escritos con `poll`.







