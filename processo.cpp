#include <iostream>
#include <windows.h>
#include <ctime>
#include <cstdlib>
#include <random>
#include <string>
#include <stdlib.h>
#define _WIN32_WINNT 0x501
#include <winsock2.h>
#include <ws2tcpip.h>
#include <cmath>    // Para usar o round
#include <cstdlib>  // Para usar o rand, srand
#include <ctime> // Para uso do time

using namespace std;

/*
###############################################################################

NÃO ESQUECA DE:

- LINKAR COM A BIBLIOTECA Ws2_32. Incluir essa opção no compilador
No CodeBlocs: botão direito no nome do projeto, Build options, Linker settings,
adicionar biblioteca Ws2_32

- Alterar o #define DEFAULT_IP_SERVER para o IP da maquina do servidor

###############################################################################
*/

/**     DEFINIÇÃO DE CONSTANTES, ESTRUTURAS DE DADOS E VARIÁVEIS GLOBAIS     **/
#define DEFAULT_PORT "27015"
#define DEFAULT_IP_SERVER "192.168.1.6"

enum STATUS
{
    STATUS_ATIVO = 1,
    STATUS_SUSPENSO = 0,
    STATUS_ENCERRAR = -1
};

enum COMANDO
{
    CMD_ENCERRAR = 0xFFFF,
    CMD_SUSPENDER = 0xEEEE,
    CMD_ATIVAR = 0x1111
};

struct Dado{
    uint16_t temp;
    uint16_t prod;
};

ostream &operator<<(ostream &O, const Dado &x)
{
    O << "T=" << x.temp << ";P=" << x.prod;
    return O;
}

struct Pacote{
    uint8_t cabecalho;
    uint16_t ID;
    Dado d;
    uint8_t rodape;
};

struct Comando{
    uint8_t cabecalho;
    uint16_t id;
    uint16_t cmd;
    uint8_t rodape;
};

/**     VARIÁVEIS GLOBAIS  **/
SOCKET ConnectSocket;
STATUS Status;
uint16_t MyID;
Dado Ultimo_dado;

/**------------- FUNÇÃO PARA GERAÇÃO DE NÚMEROS ALEATÓRIOS ------------**/
uint16_t geraAleatorio(int max, int min){
    uint16_t ale;
    srand(time(NULL));
    ale = rand()%(max - min) + min;
    return ale;
}

/**------------- THREAD PARA SIMULAÇÃO DE VALORES ------------**/
DWORD WINAPI threadSimulacao(LPVOID lpParameter){
//    //Variáveis
//    Pacote p;
//    int iResult;
//    p.cabecalho = 85;
//    p.rodape = 170;
//
//    cout << "Digite a ID do processo:  ";
//    cin >> p.ID;  //Tá parando aqui
//
//    p.d.temp = geraAleatorio(200,0);
//    p.d.porc = geraAleatorio(100,0);
//
//    while(ConnectSocket != INVALID_SOCKET){
//        iResult = send(ConnectSocket, (char*)&p, sizeof(Pacote), 0);
//        if( iResult == SOCKET_ERROR ) {
//            cerr << "Falha de envio com o erro: " << WSAGetLastError() << endl;
//            cout << "Cliente desconectado\n";
//            closesocket(ConnectSocket);
//            ConnectSocket = INVALID_SOCKET;
//        } else {
//            cout << "Bytes transmitidos: " << iResult << endl;
//        }
//        Sleep(30000);  //Espera de 30s para repetir o bloco
//    }
//
//    return 0;
    Pacote pac;   // Para enviar dados via socket
    int iResult;
    int deltaT;

    // Preenche a parte constante (que não muda) do pacote a ser enviado
    pac.cabecalho = 0x55;
    pac.rodape = 0xAA;
    pac.ID = MyID;

    while (Status!=STATUS_ENCERRAR){
        // Calcula um tempo de espera entre 30s (30.000 ms) e 3min (180.000 ms)
        deltaT = (int)round(30000.0 + 150000.0*float(rand())/RAND_MAX);

        Sleep(deltaT);

        // Simula os dados do processo
        Ultimo_dado.prod = (uint16_t)round(65535.0*float(rand())/RAND_MAX);
        Ultimo_dado.temp = (uint16_t)round(65535.0*float(rand())/RAND_MAX);

        // Soh envia os dados se estiver ATIVO
        if (Status == STATUS_ATIVO){

            // Completa o pacote a ser enviado, acrescentando a parte dinamica (variavel)
            pac.d = Ultimo_dado;

            iResult = send(ConnectSocket, (char*)&pac, sizeof(Pacote), 0);
            if ( iResult == SOCKET_ERROR ){
                cerr << "Falha de envio - erro: " << WSAGetLastError() << endl;
                cout << "Cliente desconectado\n";
                Status = STATUS_ENCERRAR;
            } else if (iResult != sizeof(Pacote)){
                cerr << "Transmissao invalida. Bytes transmitidos: " << iResult << endl;
            }
        }
    }
    return 0;
}

/**------------- THREAD PARA RECEPÇÃO DE VALORES DO SUPERVISÓRIO ------------**/
DWORD WINAPI threadRecepcao(LPVOID lpParameter){
    Comando comandoRecebido;
    int iResult;

    while (Status != STATUS_ENCERRAR){

        cout << "Aguardando comando...\n";
        // recv: receives data from a connected socket
        // Parameters:
        // s: The descriptor that identifies a connected socket.
        // buf: A pointer to the buffer to receive the incoming data.
        // len: The length, in bytes, of the buffer pointed to by the buf parameter.
        // flags: A set of flags that influences the behavior of this function.
        //iResult = recv(ConnectSocket, buf, DEFAULT_BUFLEN, 0);
        iResult = recv(ConnectSocket, (char*)&comandoRecebido, sizeof(Comando), 0);
        if ( iResult == SOCKET_ERROR ){
            cerr << "Falha na recepcao - erro: " << WSAGetLastError() << endl;
            closesocket(ConnectSocket);
            ConnectSocket = INVALID_SOCKET;
        }

        if (iResult == 0){
            cout << "Servidor desconectado\n";
            Status = STATUS_ENCERRAR;
        } else{
            if (comandoRecebido.cabecalho!=0x55 || comandoRecebido.rodape!=0xAA
                || comandoRecebido.id!=MyID){
                cerr << "Comando invalido. Descartando...\n";
            } else {
                cout << "Comando recebido: " << comandoRecebido.cmd << endl;
                switch (comandoRecebido.cmd){
                case CMD_ENCERRAR:
                    Status = STATUS_ENCERRAR;
                    break;
                case CMD_ATIVAR:
                    Status = STATUS_ATIVO;
                    break;
                case CMD_SUSPENDER:
                    Status = STATUS_SUSPENSO;
                    break;
                default:
                    cerr << "Comando desconhecido (descartando)... \n";
                    break;
                }
            }
        }
    }
    return 0;
}


int main(){  //A thread main corresponde à Simulação

    WSADATA wsaData;
    struct addrinfo hints, *result = NULL;  // para getaddrinfo, bind
    char buff[2];   // Para receber a confirmacao da conexao
    int iResult;
    int opcao;

    cout << "++++++++     PROGRAMA PROCESSO     ++++++++++" << endl;

    cout << "Iniciando o cliente...\n";
    srand(time(NULL));
    Status = STATUS_ATIVO;
    Ultimo_dado.prod = 0;
    Ultimo_dado.temp = 0;

    cout << "digite a ID do processo: ";
    cin >> MyID;

    iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
    if (iResult != 0) {
        cerr << "WSAStartup falhou: " << iResult << endl;
        return 1;
    }

    cout << "Iniciando o socket...\n";
    memset(&hints, 0, sizeof (hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    iResult = getaddrinfo(DEFAULT_IP_SERVER, DEFAULT_PORT, &hints, &result);
    if (iResult != 0) {
        cerr << "getaddrinfo falhou: " << iResult << endl;
        WSACleanup();
        return 1;
    }

    cout << "Criando o socket...\n";

    ConnectSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (ConnectSocket == INVALID_SOCKET) {
        cerr << "Erro de socket: " << WSAGetLastError() << endl;
        freeaddrinfo(result);
        WSACleanup();
        return 1;
    }

    cout << "Conectando o socket...\n";
    cout << "Conectando-se ao servidor no IP " << DEFAULT_IP_SERVER << " ...\n";

    iResult = connect( ConnectSocket, result->ai_addr, (int)result->ai_addrlen);

    freeaddrinfo(result);

    if (iResult == SOCKET_ERROR) {
        cerr << "Conexao falhou - erro: " << WSAGetLastError() << endl;
        closesocket(ConnectSocket);
        WSACleanup();
        return 1;
    }

    //Envio de id para o supervisório - validação
    iResult = send(ConnectSocket, (char*)&MyID, sizeof(MyID), 0);
    if(iResult == SOCKET_ERROR){
        cerr << "Erro de envio" << endl;
        Status = STATUS_ENCERRAR;
    }

    //Resposta do supervisório quanto à validação
    cout << "Esperando a confirmacao da conexao pelo servidor...\n";
    iResult = recv(ConnectSocket, buff, 2, 0);
    /**  Tipos de erro na resposta do supervisório  **/
    if ( iResult == SOCKET_ERROR ){
        cerr << "Falha na recepcao - erro: " << WSAGetLastError() << endl;
        closesocket(ConnectSocket);
        WSACleanup();
        return 1;
    }

    if(iResult != sizeof(MyID)){
        cerr << "ID inválida ou não existente";
        Status = STATUS_ENCERRAR;
    }

    if (iResult == 0){
        cout << "Servidor desconectado\n";
        closesocket(ConnectSocket);
        WSACleanup();
        return 1;
    }

    if (buff[0]!='O' || buff[1]!='K'){
        cout << "Conexao recusada (trocar a ID?)\n";
        closesocket(ConnectSocket);
        WSACleanup();
        return 1;
    }

    //Criação das threads
    CreateThread(NULL, 0, threadRecepcao, NULL , 0, NULL);
    CreateThread(NULL, 0, threadSimulacao, NULL , 0, NULL);


    /**     Selecao de opcoes    **/
    cout << "Pronto para receber comandos!\n\n";
    while (Status!=STATUS_ENCERRAR)
    {
        do
        {
            cout << "OPCAO [1-Imprimir; 2-Ativar; 3-Suspender; 0-Terminar]: ";
            cin >> opcao;
        } while (opcao<0 || opcao>3);
        switch (opcao)
        {
        case 0:
            Status==STATUS_ENCERRAR;
            break;
        case 1:
            cout << "STATUS " << (Status == STATUS_ATIVO ? "ATIVO" : "SUSPENSO");
            cout << "\tULTIMO DADO: " << Ultimo_dado << endl;
            break;
        case 2:
            Status==STATUS_ATIVO;
            break;
        case 3:
            Status==STATUS_SUSPENSO;
            break;
        default:
            cerr << "Opcao invalida\n";
            break;
        }
    }

    if (ConnectSocket != INVALID_SOCKET) closesocket(ConnectSocket);

    /* call WSACleanup when done using the Winsock dll */
    WSACleanup();
    return 0;
}
