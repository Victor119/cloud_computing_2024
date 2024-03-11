import java.sql.*;

public class SQLiteJDBC {
    Connection connection = null;

    public SQLiteJDBC() throws ClassNotFoundException, SQLException {
        Class.forName("org.sqlite.JDBC");
        connection = DriverManager.getConnection("jdbc:sqlite:java.db");
    }

    public void close() throws SQLException {
        this.connection.close();
    }

    private ResultSet getUser(String username) throws SQLException {
        String sql = "SELECT * FROM USERS WHERE username = ?;";

        PreparedStatement stmt = this.connection.prepareStatement(sql);
        stmt.setString(1, username);
        return stmt.executeQuery();
    }

    public boolean addUser(String username, String password) throws SQLException {
        ResultSet rs = this.getUser(username);

        if (rs.next()) {
            return false;
        }

        String sql = "INSERT INTO USERS VALUES (?, ?); ";

        PreparedStatement stmt = this.connection.prepareStatement(sql);
        stmt.setString(1, username);
        stmt.setString(2, password);
        stmt.executeUpdate();

        return true;
    }

    public boolean checkUser(String username, String password) throws SQLException {
        ResultSet rs = this.getUser(username);

        while (rs.next()) {
            String realPassword = rs.getString("password");

            if (password.equals(realPassword)) {
                return true;
            }
        }
        return false;
    }

    public boolean deleteUser(String username) throws SQLException {
        ResultSet rs = this.getUser(username);

        if (!rs.next()) {
            // Utilizatorul nu există în baza de date
            return false;
        }

        String sql = "DELETE FROM USERS WHERE username = ?;";

        try (PreparedStatement stmt = this.connection.prepareStatement(sql)) {
            stmt.setString(1, username);
            stmt.executeUpdate();
        }

        return true;
    }

    public static void main(String args[]) {
        try {
            SQLiteJDBC sqLiteJDBC = new SQLiteJDBC();
            System.out.println("Opened database successfully");

            Statement stmt = sqLiteJDBC.connection.createStatement();
            // String sql = (
            // "CREATE TABLE USERS " +
            // "(USERNAME TEXT PRIMARY KEY NOT NULL," +
            // " PASSWORD TEXT NOT NULL)"
            // );
            // stmt.executeUpdate(sql);
            stmt.close();

            System.out.println("Table created successfully");

            sqLiteJDBC.addUser("guest", "guest");
            sqLiteJDBC.addUser("jim", "jim");
            sqLiteJDBC.addUser("andrei", "andrei");
            sqLiteJDBC.addUser("alex", "alex");

        } catch (Exception e) {
            System.err.println(e.getClass().getName() + ": " + e.getMessage());
            System.exit(0);
        }
    }
}