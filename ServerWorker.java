
//import org.springframework.util.StringUtils;
//import org.apache.commons.StringUtils;
//import org.apache.commons.lang3.StringUtils;
import org.apache.commons.lang3.ArrayUtils;

import java.io.*;
import java.net.Socket;
//import java.nio.charset.StandardCharsets;
import java.sql.SQLException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
//import Server;
import java.util.stream.Collectors;

public class ServerWorker extends Thread {

    private final Socket socketClient;
    private String numeUserLogare = null;
    private final Server server;

    private HashSet<String> setareTopic = new HashSet<>();
    private OutputStream outputStream;

    // private boolean firstMessageReceived = false;
    // private boolean firstMessageSent = false;

    // splits a String into an array of substrings and vice versa
    // Splits the provided text into an array, using whitespace as the
    // separator
    public static String[] split(final String str) {
        return split(str, null, -1);
    }

    public static String[] split(final String str, final String separatorChars, final int max) {
        return splitWorker(str, separatorChars, max, false);
    }

    private static String[] splitWorker(final String str, final String separatorChars, final int max,
            final boolean preserveAllTokens) {

        if (str == null) {
            return null;
        }
        final int len = str.length();
        if (len == 0) {
            return ArrayUtils.EMPTY_STRING_ARRAY;
        }
        final List<String> list = new ArrayList<>();
        int sizePlus1 = 1;
        int i = 0;
        int start = 0;
        boolean match = false;
        boolean lastMatch = false;
        if (separatorChars == null) {
            // Null separator means use whitespace
            while (i < len) {
                if (Character.isWhitespace(str.charAt(i))) {
                    if (match || preserveAllTokens) {
                        lastMatch = true;
                        if (sizePlus1++ == max) {
                            i = len;
                            lastMatch = false;
                        }
                        list.add(str.substring(start, i));
                        match = false;
                    }
                    start = ++i;
                    continue;
                }
                lastMatch = false;
                match = true;
                i++;
            }
        } else if (separatorChars.length() == 1) {
            // Optimise 1 character case
            final char sep = separatorChars.charAt(0);
            while (i < len) {
                if (str.charAt(i) == sep) {
                    if (match || preserveAllTokens) {
                        lastMatch = true;
                        if (sizePlus1++ == max) {
                            i = len;
                            lastMatch = false;
                        }
                        list.add(str.substring(start, i));
                        match = false;
                    }
                    start = ++i;
                    continue;
                }
                lastMatch = false;
                match = true;
                i++;
            }
        } else {
            // standard case
            while (i < len) {
                if (separatorChars.indexOf(str.charAt(i)) >= 0) {
                    if (match || preserveAllTokens) {
                        lastMatch = true;
                        if (sizePlus1++ == max) {
                            i = len;
                            lastMatch = false;
                        }
                        list.add(str.substring(start, i));
                        match = false;
                    }
                    start = ++i;
                    continue;
                }
                lastMatch = false;
                match = true;
                i++;
            }
        }
        if (match || preserveAllTokens && lastMatch) {
            list.add(str.substring(start, i));
        }
        return list.toArray(ArrayUtils.EMPTY_STRING_ARRAY);
    }

    // parasim chatul leave #chatName
    private void handleLeave(String[] cuvinte) {
        if (cuvinte.length > 1) {
            String topic = cuvinte[1];
            setareTopic.remove(topic);
        }
    }

    @Override
    public void run() {
        try {
            handleClientSocket();
        } catch (IOException | InterruptedException | ClassNotFoundException | SQLException e) {
            e.printStackTrace();
        }
    }

    private void handleLogin(SQLiteJDBC sqLiteJDBC, OutputStream outputStream, String[] cuvinte)
            throws IOException, ClassNotFoundException, SQLException {
        if (cuvinte.length == 3) {
            String numeUserLogare = cuvinte[1]; // e primul cuvinte
            String secretPhrase = cuvinte[2];// e al doilea cuvinte

            // SQLiteJDBC sqLiteJDBC = new SQLiteJDBC();

            if (sqLiteJDBC.checkUser(numeUserLogare, secretPhrase)) {
                String mesaj = "200(OK): login\n";

                mesaj = mesaj.trim() + "\r\n";

                outputStream.write(mesaj.getBytes());
                this.numeUserLogare = numeUserLogare; // statul userului curent login/logoff

                // Move cursor to the beginning of the line
                String moveCursorToBeginning = "\r";
                System.out.print(moveCursorToBeginning);

                System.out.println("User signed in succesfully: " + numeUserLogare);

                // List<ServerWorker> workerList = server.getWorkerList();

                // for(ServerWorker specialist : workerList)
                // {
                // construim lista cu useri online
                // if (specialist.getLogin() != null && specialist != null)
                // {
                // if (!numeUserLogare.equals(specialist.getLogin()))
                // {
                // String mesaj2 = "online " + specialist.getLogin() + "\n";
                // trimiteMesaj(mesaj2);
                // }
                // }
                // }

                // String onlineMesaj = "online " + numeUserLogare + "\n";
                // for(ServerWorker specialist : workerList)
                // {
                // if (!numeUserLogare.equals(specialist.getLogin()))
                // {
                // //trimitem mesaj la userii care sunt logati
                // specialist.trimiteMesaj(onlineMesaj);
                // }
                // }
            } else {
                String mesaj = "404(Not Found): logare esuata\n";
                outputStream.write(mesaj.getBytes());
                System.err.println("Login failed for " + numeUserLogare);
            }

            // sqLiteJDBC.close();
        }
    }

    private void handleGet(String[] cuvinte) throws IOException {
        String[] copie_cuvinte = Arrays.copyOf(cuvinte, cuvinte.length);
        List<String> comanda = new ArrayList<>();

        for (String cuvant : copie_cuvinte) {
            // Iterează prin fiecare cuvânt din copie_cuvinte
            StringBuilder sir = new StringBuilder();
            for (int i = 0; i < cuvant.length(); i++) {
                char caracter = cuvant.charAt(i);
                if (caracter != '/') {
                    sir.append(caracter);
                } else {
                    if (sir.length() > 0) {
                        comanda.add(sir.toString());
                        sir.setLength(0);
                    }
                }
            }

            // Adaugă ultimul cuvânt din șir, dacă există
            if (sir.length() > 0) {
                comanda.add(sir.toString());
            }
        }

        // Afișează rezultatul
        System.out.println("Comanda modificata: " + comanda);

        // Verifică și tratează comanda specifică
        if (comanda.size() == 2 && "GET".equalsIgnoreCase(comanda.get(0))
                && "users".equalsIgnoreCase(comanda.get(1))) {

            // Handle GET /users (collection route)
            List<ServerWorker> workerList = server.getWorkerList();

            // Extract online users
            List<String> onlineUsers = workerList.stream()
                    .filter(worker -> worker.getLogin() != null && !worker.getLogin().equals(numeUserLogare))
                    .map(ServerWorker::getLogin)
                    .collect(Collectors.toList());

            // Construct and send the response message
            String response = "200(OK): Online users: " + String.join(", ", onlineUsers) + "\n";
            trimiteMesaj(response);

        } else if (comanda.size() == 3 && "GET".equalsIgnoreCase(comanda.get(0))
                && "users".equalsIgnoreCase(comanda.get(1))
                && "online".equalsIgnoreCase(comanda.get(2))) {

            // Handle GET /users/online (collection route)
            List<ServerWorker> workerList = server.getWorkerList();

            // Extract online users
            List<String> onlineUsers = workerList.stream()
                    .filter(worker -> worker.getLogin() != null && !worker.getLogin().equals(numeUserLogare))
                    .map(ServerWorker::getLogin)
                    .collect(Collectors.toList());

            // Construct and send the response message
            String response = "Online users: " + String.join(", ", onlineUsers) + "\n";
            trimiteMesaj(response);

        } else if (comanda.size() == 4 && "GET".equalsIgnoreCase(comanda.get(0))
                && "users".equalsIgnoreCase(comanda.get(1))
                && "status".equalsIgnoreCase(comanda.get(2))) {

            // Handle GET /users/{username}/status (resource route)
            String usernameToCheck = comanda.get(3);
            List<ServerWorker> workerList = server.getWorkerList();

            // Extract online users
            List<String> onlineUsers = workerList.stream()
                    .filter(worker -> worker.getLogin() != null && !worker.getLogin().equals(numeUserLogare))
                    .map(ServerWorker::getLogin)
                    .collect(Collectors.toList());

            // Check if the specified username is online
            boolean isUserOnline = onlineUsers.contains(usernameToCheck);

            // Construct and send the response message
            String response = usernameToCheck + ":" + (isUserOnline ? "200: online" : "200:offline") + "\n";
            trimiteMesaj(response);

        } else {
            // Handle other cases of GET requests if needed
            String response = "Unknown GET request\n";
            trimiteMesaj(response);
        }
    }

    private void handleClientSocket() throws IOException, InterruptedException, ClassNotFoundException, SQLException {

        String line;
        InputStream inputStream = socketClient.getInputStream();
        this.outputStream = socketClient.getOutputStream();

        BufferedReader peruser = new BufferedReader(new InputStreamReader(inputStream));

        SQLiteJDBC sqLiteJDBC = new SQLiteJDBC();
        // boolean firstTimeInWhile = true; // Flag to track the first time in the else
        // block

        while ((line = peruser.readLine()) != null) {
            // Remove leading spaces
            line = line.trim();

            String[] cuvinte = split(line);

            /*
             * if (firstTimeInWhile) {
             * // Elimina primele două cuvinte din array-ul cuvinte
             * if (cuvinte.length >= 2 && cuvinte != null) {
             * cuvinte = Arrays.copyOfRange(cuvinte, 2, cuvinte.length);
             * System.out.println("cuvinte dupa eliminarea primelor doua cuvinte:");
             * System.out.println(Arrays.toString(cuvinte));
             * 
             * 
             * String cuvantExtrage = cuvinte[0].toLowerCase(); // Convertește la litere
             * mici pentru comparare necase-sensitive
             * 
             * // Iterează prin caracterele cuvantExtrage folosind un for
             * for (int i = 0; i < cuvantExtrage.length(); i++) {
             * char caracter = cuvantExtrage.charAt(i);
             * 
             * // Verifică dacă caracterul curent corespunde cu unul dintre literele "g",
             * "p" sau "d"
             * if (caracter == 'g' || caracter == 'p' || caracter == 'd') {
             * // S-a găsit una dintre litere, oprind ștergerea și ieșind din buclă
             * break;
             * }
             * 
             * // Șterge caracterul curent dacă nu corespunde cu literele "g", "p" sau "d"
             * cuvantExtrage = cuvantExtrage.substring(1);
             * }
             * char caracter = cuvantExtrage.charAt(0);
             * if (caracter != 'g' && caracter != 'p' && caracter != 'd') {
             * cuvantExtrage = cuvantExtrage.substring(1);
             * }
             * 
             * char caracter2 = cuvantExtrage.charAt(0);
             * if (caracter2 != 'g' && caracter2 != 'p' && caracter2 != 'd') {
             * cuvantExtrage = cuvantExtrage.substring(1);
             * }
             * char caracter3 = cuvantExtrage.charAt(0);
             * if (caracter3 != 'g' && caracter3 != 'p' && caracter3 != 'd') {
             * cuvantExtrage = cuvantExtrage.substring(1);
             * }
             * char caracter4 = cuvantExtrage.charAt(0);
             * if (caracter4 != 'g' && caracter4 != 'p' && caracter4 != 'd') {
             * cuvantExtrage = cuvantExtrage.substring(1);
             * }
             * char caracter5 = cuvantExtrage.charAt(0);
             * if (caracter5 != 'g' && caracter5 != 'p' && caracter5 != 'd') {
             * cuvantExtrage = cuvantExtrage.substring(1);
             * }
             * char caracter6 = cuvantExtrage.charAt(0);
             * if (caracter6 != 'g' && caracter6 != 'p' && caracter6 != 'd') {
             * cuvantExtrage = cuvantExtrage.substring(1);
             * }
             * 
             * // Pune noul cuvantExtrage pe prima poziție din array-ul cuvinte
             * cuvinte[0] = cuvantExtrage;
             * 
             * // Afișează array-ul cuvinte după modificare
             * System.out.println("cuvinte dupa modificare:");
             * System.out.println(Arrays.toString(cuvinte));
             * }
             * firstTimeInWhile = false;
             * }
             */
            System.out.println("cuvinte dupa slipt");
            System.out.println(Arrays.toString(cuvinte));

            // spargem mesajul
            if (cuvinte.length > 0 && cuvinte != null) {
                String cuvinteDeVerificat = cuvinte[0];

                String comandaInserata = cuvinteDeVerificat.trim(); // extagem comanda
                // verificam ce comanda se afla in comandaInserata si daca este una din
                // urmatoarele:

                System.out.println("Comanda inserata este: ");
                System.out.println(comandaInserata);

                System.out.println("cuvinte ");
                System.out.println(Arrays.toString(cuvinte));

                if ("GET".equalsIgnoreCase(comandaInserata) || "get".equalsIgnoreCase(comandaInserata)) {
                    handleGet(cuvinte);
                }
                if ("POST".equalsIgnoreCase(comandaInserata) || "post".equalsIgnoreCase(comandaInserata)
                        || "DELETE".equalsIgnoreCase(comandaInserata) || "delete".equalsIgnoreCase(comandaInserata)) {

                    System.out.println("First time in the else block! Command: " + line);
                    String[] cuvinte2 = cuvinte;

                    System.out.println("cuvinte2 ");
                    System.out.println(Arrays.toString(cuvinte2));

                    if ("DELETE".equalsIgnoreCase(cuvinte2[0]) || "delete".equalsIgnoreCase(cuvinte2[0])) {
                        handleDelete(sqLiteJDBC, cuvinte2[1]);
                    }

                    // spargem mesajul
                    if (cuvinte2.length >= 2 && cuvinte2 != null) {
                        String cuvinteDeVerificat2 = cuvinte2[0];

                        // Elimina cuvantul "POST" de la pozitia 0
                        cuvinte2 = Arrays.copyOfRange(cuvinte2, 1, cuvinte2.length);

                        // Acum poziția 0 conține ceea ce era la poziția 1 anterior
                        cuvinteDeVerificat2 = cuvinte2.length > 0 ? cuvinte2[0] : "";

                        String comandaInserata2 = cuvinteDeVerificat2.trim(); // extagem comanda

                        System.out.println("comanda inserata :" + comandaInserata2);

                        // verificam ce comanda se afla in comandaInserata si daca este una din
                        // urmatoarele:
                        if ("logoff".equals(comandaInserata2) || "quit".equalsIgnoreCase(comandaInserata2)) {
                            handleLogoff();
                            break;
                        }
                        if ("login".equalsIgnoreCase(comandaInserata2)) {
                            handleLogin(sqLiteJDBC, outputStream, cuvinte2);
                        }
                        if ("add".equalsIgnoreCase(comandaInserata2)) {
                            // punem index 1 si 2 pentru ca s-a modificat lungimea array-ului
                            handleAdd(sqLiteJDBC, cuvinte2[1], cuvinte2[2]);
                        }

                        if ("join".equalsIgnoreCase(comandaInserata2)) {
                            handleJoin(cuvinte2);
                        }
                        if ("msg".equalsIgnoreCase(comandaInserata2)) {
                            // nu vrem sa facem split la toti tokenii ci doar la primii 3
                            String[] cuvinteMesaj2 = split(line, null, 3);
                            handleMessage(cuvinteMesaj2);
                        }
                        if ("leave".equalsIgnoreCase(comandaInserata2)) {
                            handleLeave(cuvinte2);
                        }
                    } else {
                        String mesaj = "obscure " + comandaInserata + "\n";
                        mesaj = mesaj.trim() + "\r\n";
                        outputStream.write(mesaj.getBytes());
                    }
                }

            }
        }
        socketClient.close();
    }

    private void handleDelete(SQLiteJDBC sqLiteJDBC, String username) throws SQLException, IOException {
        try {
            boolean success = sqLiteJDBC.deleteUser(username);

            if (success) {
                String response = "200(OK): User deleted successfully\n";
                response = response.trim() + "\r\n";
                outputStream.write(response.getBytes());
            } else {
                String response = "error 404: User not found\n";
                response = response.trim() + "\r\n";
                outputStream.write(response.getBytes());
            }

        } catch (SQLException e) {
            e.printStackTrace(System.err);
            String response = "error Internal server error\n";
            response = response.trim() + "\r\n";
            outputStream.write(response.getBytes());
        }
    }

    // Handling the POST command to add a user
    private void handleAdd(SQLiteJDBC sqLiteJDBC, String username, String password) throws SQLException, IOException {
        try {
            boolean success = sqLiteJDBC.addUser(username, password);

            if (success) {
                String response = "200(OK): User added successfully\n";
                response = response.trim() + "\r\n";
                outputStream.write(response.getBytes());
            } else {
                String response = "409(Conflict): error User already exists\n";
                response = response.trim() + "\r\n";
                outputStream.write(response.getBytes());
            }
        } catch (SQLException e) {
            e.printStackTrace();
            String response = "error Internal server error\n";
            response = response.trim() + "\r\n";
            outputStream.write(response.getBytes());
        }
    }

    // atribuim fiecarui user cate o instanta de server
    public ServerWorker(Server server, Socket socketClient) {
        this.server = server;
        this.socketClient = socketClient;
    }

    // adaugam membru in chat
    private void handleJoin(String[] cuvinte) {
        if (cuvinte.length > 1) {
            String topic = cuvinte[1];
            setareTopic.add(topic);
        }
    }

    private void handleMessage(String[] cuvinte) throws IOException {
        // comamda: "mesaj" "login" body...
        // comanda: "mesaj" "#numeleChatuluiInCareScriem" continutMesaj...
        String trimitemCatre = cuvinte[1];
        String continutMesaj = cuvinte[2].trim();

        boolean semnRecunoastereChatRoom = trimitemCatre.charAt(0) == '#';
        // # se afla pe prima pozitie

        List<ServerWorker> listaDeWorker = server.getWorkerList();
        for (ServerWorker specialist : listaDeWorker) {
            // verificam room in care afisam mesaj
            if (semnRecunoastereChatRoom) {
                if (specialist.apartineChatului(trimitemCatre)) {
                    String outMesaj = "msg " + trimitemCatre + ":" + numeUserLogare + " " + continutMesaj + "\n";
                    specialist.trimiteMesaj(outMesaj);
                }
            } else {
                if (trimitemCatre.equalsIgnoreCase(specialist.getLogin())) {
                    String outMesaj = "msg " + numeUserLogare + " " + continutMesaj + "\n";
                    specialist.trimiteMesaj(outMesaj);
                }
            }
        }
    }

    // verificam ca se afla in chat
    public boolean apartineChatului(String topic) {

        return setareTopic.contains(topic);
    }

    private void handleLogoff() throws IOException {
        server.removeWorker(this);
        List<ServerWorker> workerList = server.getWorkerList();

        // send other online users current user's status
        String onlineMesaj = "offline " + numeUserLogare + "\n";
        for (ServerWorker specialist : workerList) {
            if (numeUserLogare.equals(specialist.getLogin())) {
                specialist.trimiteMesaj(onlineMesaj);
            }
        }
        socketClient.close();
    }

    public String getLogin() {
        return numeUserLogare;
    }

    private void trimiteMesaj(String mesaj) throws IOException {
        if (numeUserLogare != null) {
            // Add explicit carriage return and newline characters
            mesaj = mesaj.trim() + "\r\n";
            outputStream.write(mesaj.getBytes());

            // Afisare mesaj trimis pe consolă
            System.out.println("Sent message to " + numeUserLogare + ": " + mesaj.trim());
        }
    }
}
