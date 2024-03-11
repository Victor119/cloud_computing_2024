//import com.jcraft.jsch.*;

import java.io.IOException;
import java.net.ServerSocket;
import java.net.Socket;
import java.util.ArrayList;
import java.util.List;

public class Server extends Thread
{
    private final int serverPort;

    private ArrayList<ServerWorker> workerList = new ArrayList<>();
    //aici salvam lista de clienti

    public List<ServerWorker> getWorkerList()
    {

        return workerList;
    }

    public Server(int serverPort)
    {

        this.serverPort = serverPort;
    }

    public void removeWorker(ServerWorker serverWorker)
    {

        workerList.remove(serverWorker);
    }

    @Override
    public void run() {
        try (ServerSocket serverSocket = new ServerSocket(serverPort)) {
            while (true) {
                System.out.println("About to acknowledge client connection...");
                Socket socketClient = serverSocket.accept();
                System.out.println("Accepted association from " + socketClient);
                ServerWorker specialist = new ServerWorker(this, socketClient);
                workerList.add(specialist); // adaugam in lista de useri
                specialist.start();
            }
        } catch (IOException e) {
            e.printStackTrace();
        }
    }
}