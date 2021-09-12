#include <iostream>
#include <cstring>
#include <sys/types.h>
#include <sys/select.h>
#include <fstream>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <dirent.h>
using namespace std;

bool deleteFile(string str)
{
    if (remove(str.c_str()) != 0)
		return false;
	else
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

void FileList(int client)
{
    DIR *dr;
    char buff[1024];
    struct dirent *en;
    dr = opendir("./"); //open all or present directory
    if (dr)
    {
        while ((en = readdir(dr)) != NULL)
        {
            strcpy(buff,en->d_name);
            cout<<en->d_name<<endl; //print all directory name
            send(client, buff, 1024, 0);
        }
        closedir(dr); //close all directory
        strcpy(buff, "DONE");
        send(client, buff, 1024, 0);
    }
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
            send(client, buff, nread, 0);
        }
        if (nread < 1024)
        {
            if (feof(fp))
            {
                cout<<"File transfer complete."<<endl;
            }
            if (ferror(fp))
                cout<<"Error reading"<<endl;
            break;
        }
    }
    fclose(fp);
}

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

string CodeJud(string fname, string ext)
{
    fstream file,test,res;
    string msg,txt, name = "";
    string cmd;
    int i,status,failed = 0,total = 0;
    for(i = 0; i < fname.length()-ext.length(); i++)
        name += fname[i];

    res.open("result_" + name + ".txt" , ios :: out);

    if(ext == ".cpp")
        cmd = "g++ " + fname + " -o " + name + " 2> debug_" + name + ".txt";
    if(ext == ".c")
        cmd = "gcc " + fname + " -o " + name +  " 2> debug_" + name + ".txt";
    status = system(cmd.c_str());
    res<<"\nCompilation Phase : \n";
    if(status != 0) { res<<"Compilation Error\n"; return 0; }
    else res<<"Compilation Successfull\n";

    res<<"\nExecution Phase : \n";
    txt = "input_" + name + ".txt";
    file.open(txt, ios :: in);
    if(!file)
    {
        total++;
        cout<<"Input file not present"<<endl;
        cmd = "timeout 1 ./" + name + " 1> output_" + name + ".txt";
        status = system(cmd.c_str());
        if(status == 0) res<<"Execution Successfull\n";
        else if(status >= 30000) res<<"Execution Timeout\n";
        else res<<"Execution Error\n";
    }
    else while(!file.eof())
    {
        total++;
        string str;
        getline(file,str);
      //  cout<<str<<endl;
        test.open("temp.txt", ios :: out);
        test<<str;
        test.close();
        cmd = "timeout 1 ./" + name + " 0< temp.txt 1>> output_" + name + ".txt";
        status = system(cmd.c_str());
        if(status == 0) res<<"Testcase "<<total<<" executed.\n";
        else if(status > 30000)
        {
            res<<"Testcase "<<total<<" timeout.\n";
            test.open("output_" + name + ".txt", ios :: app);
            test<<"Timeout\n";
            test.close();
        }
        else res<<"Testcase "<<total<<" error.\n";
    }
    file.close();

    file.open("output_" + name + ".txt", ios :: in);
    test.open("testcase_" + name + ".txt", ios :: in);
    while(!file.eof() && !test.eof())
    {
        string str1,str2,str = "";
        getline(file,str1);
        getline(test,str2);

        for(i = 0; i<str2.length(); i++)
            if(str2[i] != '\r') str += str2[i];

        if(strcmp(str1.c_str(),str.c_str()) != 0) failed++;
    }
    res<<"\nMatching Phase : \n";
    if(failed == 0) res<<"All testcases passed.\n";
    else res<<"Total testcases failed  : "<<failed;
    txt = "output_" + name + ".txt";
    remove(txt.c_str());
    res.close();
    return name;
}

int main(int argc, char *argv[])
{
    //Creating server socket
    int client, server, port, i;
    char fname[50] = {0};

    port = stoi(argv[1]);

    server  = socket(AF_INET, SOCK_STREAM, 0);
    if(server == -1) cout << "Error creating Server socket\n\n";

    //Assign Port address to server
    struct sockaddr_in servaddr, clientaddr;
    socklen_t len = sizeof(clientaddr);
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    servaddr.sin_addr.s_addr = INADDR_ANY;

    //Bind the server
    int status = bind(server, (struct sockaddr *) &servaddr, sizeof(servaddr));
    if(status == -1) cout << "Bind failed"<<endl;
    else cout << "Bind successfull"<<endl;

    status = listen(server, 5);
    if(status == -1) cout << "Listening failed"<<endl;
    else cout << "Listening"<<endl;

    fd_set current_sockets, ready_sockets;

    string ip[FD_SETSIZE];                      //array to store ip address
    int getport[FD_SETSIZE];                    //array to store port numbers

    FD_ZERO(&current_sockets);                  //set file descriptors to 0 initially
    FD_SET(server, &current_sockets);           //add server to file descriptor
    while(1)
    {
        int sock;
        ready_sockets = current_sockets;        //create copy since select is destructive
        if(select(FD_SETSIZE, &ready_sockets, nullptr, nullptr, nullptr) < 0)
        {
            cout<<"\nSelect Failed";
            exit(0);
        }
        for(sock = 0; sock<FD_SETSIZE; sock++)
        {
            if(FD_ISSET(sock, &ready_sockets))              //check if FD is set
            {

                if(sock == server)                          //If Fd is for server then Server will accept new clients
                {
                    //Accept Client request
                    int client = accept(server, (struct sockaddr*)&clientaddr , &len);
                    if(client < 0) cout<<"Accept failed"<<endl;
                    else cout<<"Accept successful"<<endl;

                    //get port details and start time
                    ip[client] = inet_ntoa(clientaddr.sin_addr);
                    getport[client] = ntohs(clientaddr.sin_port);
                    FD_SET(client, &current_sockets);
                }

                else                //child process will execute expressions
                {
                    char msg[256];
                    cout<<"Client : "<<sock<<", IP : "<<ip[sock]<<":"<<getport[sock]<<endl;
                    string s = "";
                    memset(msg, 0, sizeof(msg));
                    int status_recv;
                    status_recv = recv(sock, msg, 256, 0);
                    if(status_recv == 0)
                    {
                        cout<<"Client Disconnected"<<endl;
                        FD_CLR(sock, &current_sockets);
                        close(sock);
                    }
                    else
                    {
                        switch(msg[0])
                        {
                            case 'R' :
                                for(i = 2; msg[i] != '\0'; i++)
                                    s += msg[i];
                                strcpy(fname,s.c_str());
                                //cout<<"Filename : "<<fname<<endl;
                                if(filePresent(fname))
                                {
                                    strcpy(msg,"File Present");
                                    send(sock, msg, 256, 0);
                                    cout<<fname<<" present... Transferring"<<endl;
                                    SendFile(sock, fname);
                                }
                                else
                                {
                                    strcpy(msg,"File Not Present");
                                    send(sock, msg, 256, 0);
                                }
                                break;

                            case 'S' :
                                for(i = 2; msg[i] != '\0'; i++)
                                    s += msg[i];
                                strcpy(fname,s.c_str());
                                //cout<<"Filename : "<<fname<<endl;
                                if(filePresent(fname))
                                {
                                    cout<<fname<<" already present."<<endl;
                                    strcpy(msg, "File Present");
                                    send(sock, msg, 256, 0);
                                    getchar();
                                }
                                else
                                {
                                    strcpy(msg, "File not present");
                                    send(sock, msg, 256, 0);
                                    recv(sock, msg, 256, 0);
                                    if(strcmp(msg, "File Present") == 0)
                                    {
                                        ReceiveFile(sock, fname);
                                    }
                                    else cout<<fname<<" not present at client... Aborting transfer"<<endl;
                                }
                                break;

                            case 'L' :
                                FileList(sock);
                                break;

                            case 'Q' :
                                cout<<"Client closed."<<endl;
                                FD_CLR(sock, &current_sockets);
                                close(sock);
                                break;

                            case 'D' :
                                for(i = 2; msg[i] != '\0'; i++)
                                    s += msg[i];
                                strcpy(fname,s.c_str());
                                if(deleteFile(fname))
                                {
                                    cout<<fname<<" deleted successfully"<<endl;
                                    strcpy(msg, fname);
                                    strcat(msg, " deleted successfully.");
                                }
                                else
                                {
                                    cout<<fname<<" is not present. Deletion failed"<<endl;
                                    strcpy(msg, fname);
                                    strcat(msg, " deleted successfully.");
                                }
                                send(sock,msg,256,0);
                                break;
                            case 'C' :
                                string code = "",ext = "",name;
                                for(i = 2; msg[i] != '\0'; i++)
                                    code += msg[i];
                                recv(sock, msg, 256, 0);
                                ReceiveFile(sock, code);
                                name = "result_" + CodeJud(code,msg) + ".txt";

                                strcpy(msg, name.c_str());
                                send(sock, msg, 256, 0);
                                SendFile(sock, name);
                                remove(name.c_str());
                                remove(code.c_str());
                                cout<<"Codejudge successfull."<<endl;
                                break;

                        }
                        cout<<endl;
                    }
                }
            }
        }
    }
    close(server);
    return 0;
}
