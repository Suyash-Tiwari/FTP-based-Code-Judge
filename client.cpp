#include <iostream>
#include <fstream>
#include <cstring>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <netdb.h>
using namespace std;

bool valid(string name, string ext)
{
    int i,j = 1;
    if(strcmp(ext.c_str(),".c") != 0 && strcmp(ext.c_str(),".cpp") != 0) return false;
    for(i = ext.length()-1; i>=0; i--)
    {
        if(name[name.length()-j] != ext[i]) return false;
        j++;
    }
    return true;
}

bool filePresent(string str)
{
    DIR *dr;
    struct dirent *en;
    dr = opendir("./"); //open all or present directory
    if (dr)
    {
        while ((en = readdir(dr)) != NULL)
        {
            //cout<<en->d_name<<endl; //print all directory name
            if(strcmp(en->d_name, str.c_str()) == 0) return true;
        }
        closedir(dr); //close all directory
    }
    return false;
}

void ReceiveFile(int client, string s)
{
    //cout<<"Receiving "<<s<<endl;
    long int remaining;
    recv(client, &remaining, sizeof(long int), 0);
    cout<<"File Size : "<<remaining<<endl;
    int bytesReceived = 0;
    char recvBuff[1024];
    memset(recvBuff, '0', sizeof(recvBuff));

    FILE *fp = fopen(s.c_str(), "ab");

    // Receive data in chunks of 256 bytes
    while(remaining > 0)
    {
        bytesReceived = recv(client, recvBuff, 1024, 0);
        remaining -= bytesReceived;
        fflush(stdout);
        fwrite(recvBuff, sizeof(char),bytesReceived,fp);
    }
    cout<<"File OK....Completed"<<endl;
    fclose(fp);
}

void SendFile(int client, string s)
{
    //cout<<"\nSending "<<s<<endl;
    FILE* fp = fopen(s.c_str(), "rb");

    fseek(fp, 0, SEEK_END);
    long int bytes = ftell(fp);
    rewind(fp);
    send(client, &bytes, sizeof(long int), 0);
    while(1)
    {
        unsigned char buff[1024]={0};
        int nread = fread(buff,1,1024,fp);
        // If read was success, send data.
        if(nread > 0)
        {
            //printf("Sending \n");
            send(client, buff, nread, 0);
        }
        if (nread < 1024)
        {
            if (feof(fp))
            {
                cout<<"End of file"<<endl;
                cout<<"File transfer complete."<<endl;
            }
            if (ferror(fp))
                cout<<"Error reading"<<endl;
            break;
        }
    }
    fclose(fp);
}

int main(int argc, char *argv[])
{
    //Creating Client socket
    int client,port;
    int bytesReceived = 0;
    char str[256], recvBuff[1024], fname[100];
    port = stoi(argv[2]);
    client = socket(AF_INET, SOCK_STREAM, 0);
    if (client == -1) cout << "Error creating Client socket\n\n";

    struct hostent *server;
    server = gethostbyname(argv[1]);
    //Assign Port address to client
    struct sockaddr_in servaddr;
    bzero((char *)&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, (char *)&servaddr.sin_addr.s_addr, server->h_length);
    servaddr.sin_port = htons(port);

    //Connect to server
    connect(client, (struct sockaddr *)&servaddr, sizeof(servaddr));

    while(1)
    {
        memset(str, 0, sizeof(str));
        int choice;
        cout<<"\n1. RETR\n2. STOR\n3. LS\n4. QUIT\n5. DELETE\n6. CODEJUD (Eg : Filename : file.cpp\tExtension : .cpp)\n\nEnter your choice : ";
        cin>>choice;
        cin.ignore();
        switch(choice)
        {
            case 1 :
                strcpy(str, "R ");
                cout<<"Enter Filename to retrieve : ";
                cin>>fname;
                if(!filePresent(fname))
                {
                    strcat(str, fname);
                    send(client, str, 256, 0);

                    recv(client, str, 256, 0);
                    if(strcmp(str, "File Present") == 0)
                    {
                        cout<<fname<<" present. Receiving..."<<endl;
                        ReceiveFile(client, fname);
                    }
                    else cout<<str<<endl;
                }
                else cout<<fname<<" already present"<<endl;
                break;
            case 2 :
                strcpy(str, "S ");
                cout<<"Enter Filename to store : ";
                cin>>fname;
                strcat(str, fname);
                send(client, str, 256, 0);
                recv(client, str, 256, 0);

                if(strcmp(str, "File Present") == 0) cout<<fname<<" already present at server."<<endl;
                else
                {
                    if(filePresent(fname))
                    {
                        strcpy(str, "File Present");
                        cout<<fname<<" present... Transferring"<<endl;
                        send(client, str, 256, 0);
                        SendFile(client, fname);
                    }
                    else
                    {
                        cout<<"Invalid Filename";
                        strcpy(str, "File Not Present");
                        send(client, str, 256, 0);
                    }
                }
                break;
            case 3 :
                strcpy(str, "L ");
                send(client, str, 256, 0);
                while((bytesReceived = recv(client, recvBuff, 1024, 0)) > 0)
                {
                    if(strcmp(recvBuff,"DONE") == 0) break;
                    cout<<recvBuff<<endl;
                }
                break;
            case 4 :
                strcpy(str, "Q ");
                send(client, str, 256, 0);
                close(client);
                exit(0);
                break;
            case 5 :
                strcpy(str, "D ");
                cout<<"Enter Filename to delete : ";
                cin>>fname;
                strcat(str, fname);
                send(client, str, 256, 0);
                recv(client, str, 256, 0);
                cout<<str<<endl;
                break;
            case 6 :
                string code,ext;
                strcpy(str, "C ");
                cout<<"Enter Filename : ";
                cin>>code;
                cout<<"Enter Extension : ";
                cin>>ext;
                if(valid(code, ext))
                {
                    if(filePresent(code))
                    {
                        string line;
                        strcat(str,code.c_str());
                        send(client, str, 256, 0);
                        strcpy(str,ext.c_str());
                        send(client, str, 256, 0);
                        SendFile(client, code);
                        recv(client, str, 256, 0);
                        ReceiveFile(client, str);
                        cout<<endl;
                        fstream f;
                        f.open(str , ios::in);
                        if(!f) cout<<"Opening failed";
                        while(!f.eof())
                        {
                            getline(f,line);
                            cout<<line<<endl;
                        }
                        remove(str);
                    }
                    else cout<<"File not present to Judge."<<endl;
                }
                else cout<<"Invalid filename and extension combination."<<endl;


                //strcat(str, fname);
                //send(client, str, 256, 0);

                break;

        }
        if(choice == 4) break;
        cout<<endl;
    }
    //send data to server

    //strcpy(str,"This data is sent from Client\n");
    //send(client, str, 256, 0);

    //Close client socket
    return 0;
}
