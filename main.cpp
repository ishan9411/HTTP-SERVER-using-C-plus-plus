#include<bits/stdc++.h>
#include<winsock2.h>
#include<ws2tcpip.h>

#include<sstream>

#pragma comment(lib, "ws2_32.lib")

using namespace std;


struct HttpRequest{
    string method;
    string path;
    string version;
    
    unordered_map<string, string> headers;
    
    string body;
};

struct HttpResponse{
    int statusCode;
    string statusText;
    string contentType;
    
    string body;
};

struct User{
    int id;
    string name;
};

vector<User> users;
int nextUserId = 1;

HttpResponse createJsonResponse(int statusCode, string statusText, string body){
    HttpResponse response;

    response.statusCode = statusCode;
    response.statusText = statusText;
    response.contentType = "application/json";
    response.body = body;

    return response;
}

HttpResponse createResponse(int statusCode, string statusText, string body){
    HttpResponse response;
    
    response.statusCode = statusCode;
    response.statusText = statusText;
    response.contentType = "text/plain";
    response.body = body;
    
    return response;
}

string serializeResponse(const HttpResponse& response){
    string result;
    
    result += "HTTP/1.1 ";
    result += to_string(response.statusCode);
    result += " ";
    result += response.statusText;
    result += "\r\n";

    result += "Content-Type: ";
    result += response.contentType;
    result += "\r\n";
    
    result += "Content-Length: ";
    result += to_string(response.body.size());
    result += "\r\n";
    
    result += "\r\n";
    
    result += response.body;

    return result;
}

HttpRequest parseRequest(const string& rawRequest){ //we write const becoz it communicates that "this function only reads the string"
    HttpRequest request;

    istringstream stream(rawRequest);
    
    //first we will extract the request line
    string requestLine;
    getline(stream, requestLine);  //to split by anything else we can use getline(stream, requestLine, ',')

    if(!requestLine.empty() && requestLine.back() == '\r'){
        requestLine.pop_back();
    }
    
    //adding the request line parameters to HttpRequest struct
    istringstream requestLineStream(requestLine);

    requestLineStream >> request.method >> request.path >> request.version;
    
    //start reading headers
    string line;

    while(getline(stream, line)){

        if(!line.empty() && line.back() == '\r'){
            line.pop_back();
        }

        //if there is no line then it means headers are finished
        if(line.empty()){
            break;
        }

        //find colon ":"
        size_t colon = line.find(':');
        
        if(colon == string::npos){
            continue;
        }

        string key = line.substr(0, colon);
        string value = line.substr(colon+1);
        
        //we are removing any leading space. Ex - Host: localhost:8080, the leading space after colon
        if(!value.empty() && value[0] == ' '){
            value.erase(0, 1);
        }

        request.headers[key] = value;
    }
    
    //got empty line means next line body starts
    string bodyLine;

    while(getline(stream, bodyLine)){
        request.body += bodyLine;
        
        if(!stream.eof()){
            request.body += '\n';
        }
    }

    return request;
}

string escapeJson(const string& value){
    string result;

    for(char ch : value){
        if(ch == '\\' || ch == '"'){
            result += "\\";
        }

        if(ch == '\n'){
            result += "\\n";
        }
        else if(ch == '\r'){
            result += "\\r";
        }
        else if(ch == '\t'){
            result += "\\t";
        }
        else{
            result += ch;
        }
    }

    return result;
}

//using Handler = HttpResponse (*)(const HttpRequest&); //we can also use function pointers -> will not support capturing lambdas, but is blazing fast(direct jump to CPU)
using Handler = function<HttpResponse(const HttpRequest&)>;

class Router{
    private:
        unordered_map<string, Handler> routes;

    public:
        void addRoute(const string& method,const string& path, Handler handler){
            string key = method + " " + path;

            routes[key] = handler;
        }

        HttpResponse route(const HttpRequest& request){
            string key = request.method + " " + request.path;

            auto it = routes.find(key);

            if(it == routes.end()){
                return createResponse(404, "Not Found", "Route not found");
            }

            return it->second(request);
        }
};

HttpResponse homeHandler(const HttpRequest& request){
    return createResponse(200, "OK", "Welcome to HTTP Server!");
}
HttpResponse healthHandler(const HttpRequest& request){
    return createResponse(200, "OK", "Server is healthy :)");
}
HttpResponse aboutHandler(const HttpRequest& request){
    return createResponse(200, "OK", "This is an c++ backend engineering of server.");
}

//making this initially where in request we are only sending name -> --data-binary "Ishan"
HttpResponse createUserHandler(const HttpRequest& request){
    string name = request.body;

    int start = name.find_first_not_of(" \t\r\n");
    int end  = name.find_last_not_of(" \t\r\n");

    if(start == string::npos){
        return createResponse(400, "Bad Request", "Name is required");
    }

    name = name.substr(start, end-start+1);

    User user{nextUserId++, name};
    users.push_back(user);

    string body = "{\"id\":" + to_string(user.id) + ",\"name\":\"" + escapeJson(user.name) + "\"}";

    return createJsonResponse(201, "Created", body);
}

HttpResponse getUserHandler(const HttpRequest& request){
    string body = "[";

    for(int i = 0; i<users.size(); i++){
        if(i > 0){
            body += ",";
        }

        body += "{\"id\":" + to_string(users[i].id);
        body += ",\"name\":\"" + escapeJson(users[i].name) + "\"}";
    }

    body += "]";

    return createJsonResponse(200, "OK", body);
}

int main(){
    //Initializing winsock
    WSADATA wsaData;
    
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);

    if(result != 0){
        cerr << endl << "WSA Startup failed";
        return 1;
    }
    
    cout << endl << "Winsock initialized. \n";

    //now we will create socket
    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, 0);

    if(serverSocket == INVALID_SOCKET){
        cerr << "Socket creation failed\n";
        WSACleanup();
        return 1;
    }

    cout << "Socket created. \n";

    //here we wil configure the socket address and everything
    sockaddr_in serverAddress{};  //sockaddr_in is a struct provided by winsock

    serverAddress.sin_family = AF_INET;       //IPv4
    serverAddress.sin_addr.s_addr = INADDR_ANY;   //local IP address
    serverAddress.sin_port = htons(8080);        //Port Number for server in network byte order

    //now bind this serverSocket to the parameters of socket address
    result = bind(serverSocket, reinterpret_cast<sockaddr*>(&serverAddress), sizeof(serverAddress));

    if(result == SOCKET_ERROR){
        cerr << "Bind failed\n";

        closesocket(serverSocket);
        WSACleanup();

        return 1;
    }

    cout << "Socket bound to port 8080.\n";

    //now we will try to listen on the serverSocket for any incoming request from clients
    result = listen(serverSocket, SOMAXCONN);

    if(result == SOCKET_ERROR){
        std::cerr << "Listen failed\n";

        closesocket(serverSocket);
        WSACleanup();

        return 1;
    }

    cout << "Server listening on port 8080...\n";

    Router router;

    router.addRoute("GET", "/", homeHandler);
    router.addRoute("GET", "/health", healthHandler);
    router.addRoute("GET", "/about", aboutHandler);

    router.addRoute("GET", "/users", getUserHandler);
    router.addRoute("POST", "/users", createUserHandler);

    while(true){

        cout << "Waiting For client..." << endl;

        //now lets make a client
        sockaddr_in clientAddress{};
        int clientAddressSize = sizeof(clientAddress);
    
        //accept a client
        SOCKET clientSocket = accept(serverSocket, reinterpret_cast<sockaddr*>(&clientAddress), &clientAddressSize);
    
        if(clientSocket == INVALID_SOCKET){
            cerr << "Accept failed\n";
    
            closesocket(serverSocket);
            WSACleanup();
    
            return 1;
        }
    
        cout << "Client connected!\n";
    
        //now to receive http request from client
        char buffer[4096] = {};
    
        int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
    
        if (bytesReceived > 0)
        {
            string rawRequest(buffer, bytesReceived);

            HttpRequest request = parseRequest(rawRequest);

            HttpResponse response = router.route(request);

            string responseText = serializeResponse(response);

            int bytesSent = send(clientSocket, responseText.c_str(), static_cast<int>(responseText.size()), 0);
    
            if(bytesSent == SOCKET_ERROR){
                cerr << "Send failed.\n";
            }

            cout << "\nParsed Request:\n";

            cout << "Method:" << request.method << endl;
            cout << "Path:" << request.path << endl;
            cout << "Version:" << request.version << endl;

            for(auto& [key, value] : request.headers){
                cout << key << ": " << value << endl;
            }

            cout << "Body: " << request.body << endl;
        }
        else if (bytesReceived == 0)
        {
            cout << "Client disconnected.\n";
        }
        else
        {
            cerr << "Receive failed.\n";
        }
        
    
        // Cleanup
        closesocket(clientSocket);
    }

    closesocket(serverSocket);

    WSACleanup();

    cout << "Server shut down.\n";

    return 0;
}