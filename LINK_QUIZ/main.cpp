// Standard C++ Libraries
#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <mysql.h>
#include <conio.h>

using namespace std;

// Forward declarations
class User;
class Question;
class Quiz;
class DatabaseManager;

//helper function for password  inut
string getHiddenInput(const string& prompt) {
    cout << prompt;
    string input;
    char ch;
    while ((ch = _getch()) != '\r') { // Read until Enter key
        if (ch == '\b') { // Handle backspace
            if (!input.empty()) {
                input.pop_back();
                cout << "\b \b"; // Move back, overwrite with space, move back again
            }
        } else {
            input.push_back(ch);
            cout << '*';
        }
    }
    cout << endl;
    return input;
}

// Base User class
// Abstract base class for all users (Admin and Student)
class User {
protected:
    int id;
    string username;
    string password;
    string role;

public:
    User(int id, const string& username, const string& password, const string& role)
        : id(id), username(username), password(password), role(role) {}

    virtual ~User() {}

    int getId() const { return id; }
    string getUsername() const { return username; }
    string getRole() const { return role; }

    virtual void displayMenu(DatabaseManager& db) = 0;
    bool authenticate(const string& inputPassword) const {
        return password == inputPassword;
    }
};

// Admin class derived from User
// Admin class with privileges to create/delete quizzes and questions
class Admin : public User {
public:
    Admin(int id, const string& username, const string& password)
        : User(id, username, password, "admin") {}

    void displayMenu(DatabaseManager& db) override;
};

// Student class derived from User
// Student class which can take quizzes and view score and rank
class Student : public User {
private:
    int score;
    vector<int> completedQuizzes;

public:
    Student(int id, const string& username, const string& password)
        : User(id, username, password, "student"), score(0) {}

    void displayMenu(DatabaseManager& db) override;
    void takeQuiz(Quiz& quiz);
    void updateScore(int points) { score += points; }
    int getScore() const { return score; }
};

// Question class
class Question {
private:
    int id;
    string text;
    vector<string> options;
    int correctOption;
    int quizId;

public:
    Question(int id, const string& text, const vector<string>& options,
             int correctOption, int quizId)
        : id(id), text(text), options(options), correctOption(correctOption), quizId(quizId) {}

    int getId() const { return id; }
    string getText() const { return text; }
    vector<string> getOptions() const { return options; }
    int getCorrectOption() const { return correctOption; }
    int getQuizId() const { return quizId; }

    bool checkAnswer(int userChoice) const {
        return userChoice == correctOption;
    }

    void display() const {
        cout << "\nQuestion: " << text << "\n";
        for (size_t i = 0; i < options.size(); ++i) {
            cout << i + 1 << ". " << options[i] << "\n";
        }
    }
};

// Quiz class
// Quiz class holds quiz metadata and questions
class Quiz {
private:
    int id;
    string title;
    string description;
    vector<Question> questions;

public:
    Quiz(int id, const string& title, const string& description)
        : id(id), title(title), description(description) {}

    int getId() const { return id; }
    string getTitle() const { return title; }
    string getDescription() const { return description; }
    const vector<Question>& getQuestions() const { return questions; }

    void addQuestion(const Question& question) {
        questions.push_back(question);
    }

    void display() const {
        cout << "\nQuiz: " << title << "\n";
        cout << "Description: " << description << "\n";
        cout << "Number of Questions: " << questions.size() << "\n";
    }

    void startQuiz(Student& student) {
        int score = 0;
        cout << "\nStarting Quiz: " << title << "\n";

        for (const auto& question : questions) {
            question.display();
            cout << "Your answer (1-" << question.getOptions().size() << "): ";
            int choice;
            cin >> choice;

            if (question.checkAnswer(choice)) {
                cout << "Correct!\n";
                score++;
            } else {
                cout << "Incorrect. The correct answer was: " << question.getCorrectOption() << "\n";
            }
        }

        cout << "\nQuiz completed! Your score: " << score << "/" << questions.size() << "\n";
        student.updateScore(score);
    }
};

    struct UserRole {
        int id;
        string role;
    };

// Database Manager class
// DatabaseManager handles all MySQL interactions
class DatabaseManager {
private:
    MYSQL* conn;
    string server;
    string user;
    string password;
    string database;

public:
    DatabaseManager(const string& server, const string& user,
                   const string& password, const string& database)
        : server(server), user(user), password(password), database(database) {
        conn = mysql_init(nullptr);
        if (!conn) {
            cerr << "MySQL initialization failed" << endl;
            exit(1);
        }

        if (!mysql_real_connect(conn, server.c_str(), user.c_str(),
                              password.c_str(), database.c_str(), 0, nullptr, 0)) {
            cerr << "Connection Error: " << mysql_error(conn) << endl;
            exit(1);
        }

        initializeDatabase();
    }

    ~DatabaseManager() {
        mysql_close(conn);
    }

    void executeQuery(const string& query) {
        if (mysql_query(conn, query.c_str())) {
            cerr << "MySQL Query Error: " << mysql_error(conn) << endl;
        }
    }

    MYSQL_RES* executeQueryWithResult(const string& query) {
        if (mysql_query(conn, query.c_str())) {
            cerr << "MySQL Query Error: " << mysql_error(conn) << endl;
            return nullptr;
        }
        return mysql_store_result(conn);
    }

    void initializeDatabase() {
        // Create tables if they don't exist
        vector<string> createTables = {
            "CREATE TABLE IF NOT EXISTS users ("
            "id INT AUTO_INCREMENT PRIMARY KEY,"
            "username VARCHAR(50) NOT NULL,"
            "password VARCHAR(100) NOT NULL,"
            "role ENUM('admin', 'student') NOT NULL,"
            "score INT DEFAULT 0,"
            "CONSTRAINT username_role_unique UNIQUE (username, role))",

            "CREATE TABLE IF NOT EXISTS quizzes ("
            "id INT AUTO_INCREMENT PRIMARY KEY,"
            "title VARCHAR(100) NOT NULL,"
            "description TEXT,"
            "time_limit INT)",

            "CREATE TABLE IF NOT EXISTS questions ("
            "id INT AUTO_INCREMENT PRIMARY KEY,"
            "quiz_id INT NOT NULL,"
            "text TEXT NOT NULL,"
            "option1 TEXT NOT NULL,"
            "option2 TEXT NOT NULL,"
            "option3 TEXT,"
            "option4 TEXT,"
            "correct_option INT NOT NULL,"
            "FOREIGN KEY (quiz_id) REFERENCES quizzes(id) ON DELETE CASCADE)",

            "CREATE TABLE IF NOT EXISTS student_quizzes ("
            "student_id INT NOT NULL,"
            "quiz_id INT NOT NULL,"
            "score INT NOT NULL,"
            "completed_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
            "PRIMARY KEY (student_id, quiz_id),"
            "FOREIGN KEY (student_id) REFERENCES users(id) ON DELETE CASCADE,"
            "FOREIGN KEY (quiz_id) REFERENCES quizzes(id) ON DELETE CASCADE)"
        };

        for (const auto& query : createTables) {
            executeQuery(query);
        }
    }

    unique_ptr<User> authenticateUser(const string& username, const string& password) {
        string query = "SELECT id, username, password, role, score FROM users WHERE username = '" +
                      escapeString(username) + "'";

        MYSQL_RES* result = executeQueryWithResult(query);
        if (!result) return nullptr;

        MYSQL_ROW row = mysql_fetch_row(result);
        if (row) {
            int id = stoi(row[0]);
            string dbUsername = row[1];
            string dbPassword = row[2];
            string role = row[3];

            if (dbPassword == password) {
                mysql_free_result(result);

                if (role == "admin") {
                    return std::make_unique<Admin>(id, dbUsername, dbPassword);
                } else {
                    auto student = std::make_unique<Student>(id, dbUsername, dbPassword);
                    student->updateScore(row[4] ? stoi(row[4]) : 0);
                    return std::unique_ptr<User>(std::move(student));
                }
            }
        }

        mysql_free_result(result);
        return nullptr;
    }


bool registerUser(const string& username, const string& password, const string& role) {
    // Check if username+role combination already exists
    string checkQuery = "SELECT id FROM users WHERE username = '" + escapeString(username) +
                       "' AND role = '" + escapeString(role) + "'";

    MYSQL_RES* result = executeQueryWithResult(checkQuery);
    if (result && mysql_num_rows(result) > 0) {
        mysql_free_result(result);
        return false; // Username already exists for this specific role
    }
    if (result) mysql_free_result(result);

    // Insert new user
    string query = "INSERT INTO users (username, password, role) VALUES ('" +
                  escapeString(username) + "', '" +
                  escapeString(password) + "', '" +
                  escapeString(role) + "')";

    return mysql_query(conn, query.c_str()) == 0;
}

    vector<Quiz> getAllQuizzes() {
        vector<Quiz> quizzes;
        string query = "SELECT id, title, description, time_limit FROM quizzes";

        MYSQL_RES* result = executeQueryWithResult(query);
        if (!result) return quizzes;

        MYSQL_ROW row;
        while ((row = mysql_fetch_row(result))) {
            int id = stoi(row[0]);
            string title = row[1] ? row[1] : "";
            string description = row[2] ? row[2] : "";
            int timeLimit = row[3] ? stoi(row[3]) : 0;

            Quiz quiz(id, title, description);

            // Load questions for this quiz
            string questionQuery = "SELECT id, text, option1, option2, option3, option4, correct_option "
                                 "FROM questions WHERE quiz_id = " + to_string(id);
            MYSQL_RES* questionResult = executeQueryWithResult(questionQuery);

            if (questionResult) {
                MYSQL_ROW questionRow;
                while ((questionRow = mysql_fetch_row(questionResult))) {
                    int qid = stoi(questionRow[0]);
                    string text = questionRow[1] ? questionRow[1] : "";

                    vector<string> options;
                    options.push_back(questionRow[2] ? questionRow[2] : "");
                    options.push_back(questionRow[3] ? questionRow[3] : "");

                    if (questionRow[4]) options.push_back(questionRow[4]);
                    if (questionRow[5]) options.push_back(questionRow[5]);

                    int correctOption = questionRow[6] ? stoi(questionRow[6]) : 1;

                    quiz.addQuestion(Question(qid, text, options, correctOption, id));
                }
                mysql_free_result(questionResult);
            }

            quizzes.push_back(quiz);
        }

        mysql_free_result(result);
        return quizzes;
    }

    bool addQuiz(const Quiz& quiz) {
        string query = "INSERT INTO quizzes (title, description) VALUES ('" +
                  escapeString(quiz.getTitle()) + "', '" +
                  escapeString(quiz.getDescription()) + "')";

        if (mysql_query(conn, query.c_str())) {
            cerr << "Error: " << mysql_error(conn) << endl;
            return false;
        }

        int quizId = static_cast<int>(mysql_insert_id(conn));

        // Add questions
        for (const auto& question : quiz.getQuestions()) {
            addQuestion(quizId, question);
        }

        return true;
    }

    bool addQuestion(int quizId, const Question& question) {
        string query = "INSERT INTO questions (quiz_id, text, option1, option2, option3, option4, correct_option) VALUES (" +
                      to_string(quizId) + ", '" +
                      escapeString(question.getText()) + "', '" +
                      escapeString(question.getOptions()[0]) + "', '" +
                      escapeString(question.getOptions()[1]) + "', " ;

        // Handle optional options
        if (question.getOptions().size() > 2) {
            query += "'" + escapeString(question.getOptions()[2]) + "', ";
        } else {
            query += "NULL, ";
        }

        if (question.getOptions().size() > 3) {
            query += "'" + escapeString(question.getOptions()[3]) + "', ";
        } else {
            query += "NULL, ";
        }

        query += to_string(question.getCorrectOption()) + ")";

        if (mysql_query(conn, query.c_str())) {
            cerr << "Error: " << mysql_error(conn) << endl;
            return false;
        }
        return true;
    }

    bool recordQuizAttempt(int studentId, int quizId, int score) {
        string query = "INSERT INTO student_quizzes (student_id, quiz_id, score) VALUES (" +
                      to_string(studentId) + ", " +
                      to_string(quizId) + ", " +
                      to_string(score) + ") "
                      "ON DUPLICATE KEY UPDATE score = VALUES(score)";

        if (mysql_query(conn, query.c_str())) {
            cerr << "Error: " << mysql_error(conn) << endl;
            return false;
        }

        // Update user's total score
        query = "UPDATE users SET score = score + " + to_string(score) +
               " WHERE id = " + to_string(studentId);

        if (mysql_query(conn, query.c_str())) {
            cerr << "Error: " << mysql_error(conn) << endl;
            return false;
        }

        return true;
    }

    string escapeString(const string& input) {
        char* output = new char[input.length() * 2 + 1];
        mysql_real_escape_string(conn, output, input.c_str(), input.length());
        string result(output);
        delete[] output;
        return result;
    }

    bool deleteUserAccount(int userId, const string& role = "") {
        string query = "DELETE FROM users WHERE id = " + to_string(userId);
        if (!role.empty()) {
            query += " AND role = '" + escapeString(role) + "'";
        }

        if (mysql_query(conn, query.c_str())) {
            cerr << "Error deleting user: " << mysql_error(conn) << endl;
            return false;
        }

        if (mysql_affected_rows(conn) == 0) {
            return false; // No such user+role found
        }
        return true;
    }
    
    vector<UserRole> getUserRoles(const string& username) {
        vector<UserRole> roles;
        string query = "SELECT id, role FROM users WHERE username = '" + 
                        escapeString(username) + "'";
    
        MYSQL_RES* result = executeQueryWithResult(query);
        if (result) {
            MYSQL_ROW row;
            while ((row = mysql_fetch_row(result))) {
                roles.push_back({stoi(row[0]), row[1]});
            }
            mysql_free_result(result);
        }
        return roles;
    }

    vector<UserRole> getUserRoles(const string& username, const string& password) {
        vector<UserRole> roles;
        string query = "SELECT id, role FROM users WHERE username = '" + 
                      escapeString(username) + "' AND password = '" + 
                      escapeString(password) + "'";
    
        MYSQL_RES* result = executeQueryWithResult(query);
        if (result) {
            MYSQL_ROW row;
            while ((row = mysql_fetch_row(result))) {
                roles.push_back({stoi(row[0]), row[1]});
            }
            mysql_free_result(result);
        }
        return roles;
    }

vector<UserRole> getAllRolesForUser(const string& username) {
    vector<UserRole> roles;
    string query = "SELECT id, role FROM users WHERE username = '" + 
                 escapeString(username) + "'";
    
    MYSQL_RES* result = executeQueryWithResult(query);
    if (result) {
        MYSQL_ROW row;
        while ((row = mysql_fetch_row(result))) {
            roles.push_back({stoi(row[0]), row[1]});
        }
        mysql_free_result(result);
    }
    return roles;
}

bool verifyPassword(const string& username, const string& password) {
    string query = "SELECT 1 FROM users WHERE username = '" +
                  escapeString(username) + "' AND password = '" +
                  escapeString(password) + "' LIMIT 1";

    MYSQL_RES* result = executeQueryWithResult(query);
    if (result) {
        bool valid = mysql_num_rows(result) > 0;
        mysql_free_result(result);
        return valid;
    }
    return false;
}

bool deleteQuiz(int quizId) {
    string query = "DELETE FROM quizzes WHERE id = " + to_string(quizId);
    if (mysql_query(conn, query.c_str())) {
        cerr << "Error deleting quiz: " << mysql_error(conn) << endl;
        return false;
    }
    return true;
}

bool deleteQuestion(int questionId) {
    string query = "DELETE FROM questions WHERE id = " + to_string(questionId);
    if (mysql_query(conn, query.c_str())) {
        cerr << "Error deleting question: " << mysql_error(conn) << endl;
        return false;
    }
    return true;
}

// Display a ranked leaderboard of all students and show the rank of the current student
void displayStudentRanks(int currentStudentId) {
    string query = "SELECT id, username, score FROM users WHERE role = 'student' ORDER BY score DESC, username ASC";
    MYSQL_RES* result = executeQueryWithResult(query);
    if (!result) return;

    MYSQL_ROW row;
    int rank = 0;
    int currentRank = -1;
    cout << "\n--- Student Leaderboard ---\n";
    cout << "Rank\tUsername\tScore\n";

    while ((row = mysql_fetch_row(result))) {
        ++rank;
        int id = stoi(row[0]);
        string username = row[1] ? row[1] : "";
        int score = row[2] ? stoi(row[2]) : 0;

        cout << rank << "\t" << username << "\t\t" << score << "\n";

        if (id == currentStudentId) {
            currentRank = rank;
        }
    }
    mysql_free_result(result);

    if (currentRank != -1) {
        cout << "\nYour rank is: " << currentRank << "\n";
    } else {
        cout << "\nYou are not ranked (no score recorded yet).\n";
    }
}


};

// Admin menu implementation
void Admin::displayMenu(DatabaseManager& db) {
    while (true) {
        cout << "\nAdmin Menu\n";
        cout << "1. Create Quiz\n";
        cout << "2. View All Quizzes\n";
        cout << "3. Delete a Quiz\n";
        cout << "4. Delete a Question from a Quiz\n";
        cout << "5. Add Question to Existing Quiz\n";
        cout << "6. Logout\n";
        cout << "Enter your choice: ";

        int choice;
        cin >> choice;
        cin.ignore(); // Clear newline

        switch (choice) {
            case 1: {
                string title, description;
                int timeLimit;

                cout << "Enter quiz title: ";
                getline(cin, title);
                cout << "Enter quiz description: ";
                getline(cin, description);

                Quiz newQuiz(0, title, description);

                int questionCount;
                cout << "How many questions? ";
                cin >> questionCount;
                cin.ignore();

                for (int i = 0; i < questionCount; ++i) {
                    string text;
                    vector<string> options;
                    int correctOption;

                    cout << "\nQuestion " << i + 1 << ": ";
                    getline(cin, text);

                    for (int j = 0; j < 4; ++j) {
                        string option;
                        cout << "Option " << j + 1 << ": ";
                        getline(cin, option);
                        if (!option.empty()) {
                            options.push_back(option);
                        } else {
                            break;
                        }
                    }

                    while (true) {
                        cout << "Correct option (1-" << options.size() << "): ";
                        cin >> correctOption;
                        cin.ignore();
                        if (correctOption >= 1 && correctOption <= static_cast<int>(options.size())) {
                            break;
                        } else {
                              cout << "Invalid input. Please enter a number between 1 and " << options.size() << ".\n";
                            }
                        }
                    newQuiz.addQuestion(Question(0, text, options, correctOption, 0));
                }

                if (db.addQuiz(newQuiz)) {
                    cout << "Quiz added successfully!\n";
                } else {
                    cout << "Failed to add quiz.\n";
                }
                break;
            }
            case 2: {
                auto quizzes = db.getAllQuizzes();
                if (quizzes.empty()) {
                    cout << "\nNo quizzes found.\n";
                    char create;
                    cout << "Would you like to create a new quiz? (y/n): ";
                    cin >> create;
                    cin.ignore();
                    if (create == 'y' || create == 'Y') {
                        // Directly go to quiz creation without returning to menu
                        string title, description;
                        int timeLimit;

                        cout << "\nEnter quiz title: ";
                        getline(cin, title);
                        cout << "Enter quiz description: ";
                        getline(cin, description);

                        Quiz newQuiz(0, title, description);

                        int questionCount;
                        cout << "How many questions? ";
                        cin >> questionCount;
                        cin.ignore();

                        for (int i = 0; i < questionCount; ++i) {
                            string text;
                            vector<string> options;
                            int correctOption;

                            cout << "\nQuestion " << i + 1 << ": ";
                            getline(cin, text);

                            for (int j = 0; j < 4; ++j) {
                                string option;
                                cout << "Option " << j + 1 << ": ";
                                getline(cin, option);
                                if (!option.empty()) {
                                    options.push_back(option);
                                } else {
                                    break;
                                }
                            }

                    while (true) {
                        cout << "Correct option (1-" << options.size() << "): ";
                        cin >> correctOption;
                        cin.ignore();
                        if (correctOption >= 1 && correctOption <= static_cast<int>(options.size())) {
                            break;
                        } else {
                              cout << "Invalid input. Please enter a number between 1 and " << options.size() << ".\n";
                            }
                        }
                            newQuiz.addQuestion(Question(0, text, options, correctOption, 0));
                        }

                        if (db.addQuiz(newQuiz)) {
                            cout << "Quiz added successfully!\n";
                        } else {
                            cout << "Failed to add quiz.\n";
                        }
                    }
                    // If 'n' was chosen, it will naturally return to the admin menu
                } else {
                    cout << "\nAll Quizzes:\n";
                    for (const auto& quiz : quizzes) {
                        quiz.display();
                    }
                }
                break;
            }
            
            case 3: {
    auto quizzes = db.getAllQuizzes();
    if (quizzes.empty()) {
        cout << "No quizzes available to delete.\n";
        break;
    }

    cout << "\nSelect a quiz to delete:\n";
    for (size_t i = 0; i < quizzes.size(); ++i) {
        cout << i + 1 << ". " << quizzes[i].getTitle() << "\n";
    }

    int quizChoice;
    cout << "Enter your choice (1-" << quizzes.size() << "): ";
    cin >> quizChoice;

    if (quizChoice >= 1 && quizChoice <= static_cast<int>(quizzes.size())) {
        int quizId = quizzes[quizChoice - 1].getId();
        if (db.deleteQuiz(quizId)) {
            cout << "Quiz deleted successfully.\n";
        } else {
            cout << "Failed to delete quiz.\n";
        }
    } else {
        cout << "Invalid choice.\n";
    }
    break;
}

case 4: {
    auto quizzes = db.getAllQuizzes();
    if (quizzes.empty()) {
        cout << "No quizzes available.\n";
        break;
    }

    cout << "\nSelect a quiz:\n";
    for (size_t i = 0; i < quizzes.size(); ++i) {
        cout << i + 1 << ". " << quizzes[i].getTitle() << "\n";
    }

    int quizChoice;
    cout << "Enter your choice (1-" << quizzes.size() << "): ";
    cin >> quizChoice;

    if (quizChoice >= 1 && quizChoice <= static_cast<int>(quizzes.size())) {
        const auto& selectedQuiz = quizzes[quizChoice - 1];
        const auto& questions = selectedQuiz.getQuestions();
        if (questions.empty()) {
            cout << "No questions in this quiz.\n";
            break;
        }

        cout << "\nSelect a question to delete:\n";
        for (size_t i = 0; i < questions.size(); ++i) {
            cout << i + 1 << ". " << questions[i].getText() << "\n";
        }

        int qChoice;
        cout << "Enter your choice (1-" << questions.size() << "): ";
        cin >> qChoice;

        if (qChoice >= 1 && qChoice <= static_cast<int>(questions.size())) {
            int questionId = questions[qChoice - 1].getId();
            if (db.deleteQuestion(questionId)) {
                cout << "Question deleted successfully.\n";
            } else {
                cout << "Failed to delete question.\n";
            }
        } else {
            cout << "Invalid choice.\n";
        }
    } else {
        cout << "Invalid choice.\n";
    }
    break;
}

            case 5: {  // New case for adding question to existing quiz
                auto quizzes = db.getAllQuizzes();
                if (quizzes.empty()) {
                    cout << "No quizzes available to add questions to.\n";
                    break;
                }

                cout << "\nSelect a quiz to add a question to:\n";
                for (size_t i = 0; i < quizzes.size(); ++i) {
                    cout << i + 1 << ". " << quizzes[i].getTitle() << "\n";
                }

                int quizChoice;
                cout << "Enter your choice (1-" << quizzes.size() << "): ";
                cin >> quizChoice;
                cin.ignore();

                if (quizChoice >= 1 && quizChoice <= static_cast<int>(quizzes.size())) {
                    int quizId = quizzes[quizChoice - 1].getId();
                    
                    string text;
                    vector<string> options;
                    int correctOption;

                    cout << "\nEnter the question text: ";
                    getline(cin, text);

                    for (int j = 0; j < 4; ++j) {
                        string option;
                        cout << "Option " << j + 1 << ": ";
                        getline(cin, option);
                        if (!option.empty()) {
                            options.push_back(option);
                        } else {
                            break;
                        }
                    }

                    while (true) {
                        cout << "Correct option (1-" << options.size() << "): ";
                        cin >> correctOption;
                        cin.ignore();
                        if (correctOption >= 1 && correctOption <= static_cast<int>(options.size())) {
                            break;
                        } else {
                              cout << "Invalid input. Please enter a number between 1 and " << options.size() << ".\n";
                            }
                        }

                    // Create a temporary question object
                    Question newQuestion(0, text, options, correctOption, quizId);
                    
                    if (db.addQuestion(quizId, newQuestion)) {
                        cout << "Question added successfully!\n";
                    } else {
                        cout << "Failed to add question.\n";
                    }
                } else {
                    cout << "Invalid choice.\n";
                }
                break;
            }
            
            case 6:
                return;
            default:
                cout << "Invalid choice. Try again.\n";
        }
    }
}

// Student menu implementation
void Student::displayMenu(DatabaseManager& db) {
    while (true) {
        cout << "\nStudent Menu\n";
        cout << "1. Take a Quiz\n";
        cout << "2. View My Score\n";
        cout << "3. View My Rank\n";
        cout << "4. View Available Quizzes\n";
        cout << "5. Logout\n";
        cout << "Enter your choice: ";

        int choice;
        cin >> choice;

        switch (choice) {
            case 1: {
                auto quizzes = db.getAllQuizzes();
                if (quizzes.empty()) {
                    cout << "No quizzes available at the moment, please check back later!!!!.\n";
                    break;
                }

                cout << "\nAvailable Quizzes:\n";
                for (size_t i = 0; i < quizzes.size(); ++i) {
                    cout << i + 1 << ". " << quizzes[i].getTitle() << "\n";
                }

                cout << "Select a quiz to take (1-" << quizzes.size() << "): ";
                int quizChoice;
                cin >> quizChoice;

                if (quizChoice > 0 && quizChoice <= static_cast<int>(quizzes.size())) {
                    quizzes[quizChoice - 1].startQuiz(*this);
                    db.recordQuizAttempt(id, quizzes[quizChoice - 1].getId(), score);
                } else {
                    cout << "Invalid choice.\n";
                }
                break;
            }
            case 2:
                cout << "\nYour total score: " << score << "\n";
                break;
                
            case 3:
                db.displayStudentRanks(id);
                break;    
            case 4: {
                auto quizzes = db.getAllQuizzes();
    if (quizzes.empty()) {
        cout << "\nNo quizzes are currently available.\n";
    } else {
        cout << "\nAvailable Quizzes:\n";
        for (const auto& quiz : quizzes) {
            quiz.display();
        }
    }
    break;
            }
            case 5:
                return;
            default:
                cout << "Invalid choice. Try again.\n";
        }
    }
}

void deleteAccountFlow(DatabaseManager& db) {
    string username, password;
    cout << "\n=== Delete Account ===\n";
    cout << "Enter your username (or 'cancel' to exit): ";
    cin >> username;
    if (username == "cancel") return;

    // First check if username exists in any role
    auto userRoles = db.getUserRoles(username);
    if (userRoles.empty()) {
        cout << "No account found for this username.\n";
        return;
    }

    // Now verify password
    cout << "\nEnter your password to proceed or type 'cancel' to exit.\n";
    while (true) {
        password = getHiddenInput("Password: ");
        if (password == "cancel") return;

        // Get roles that match both username AND password
        auto validRoles = db.getUserRoles(username, password);
        if (!validRoles.empty()) {
            userRoles = validRoles; // Update with password-verified roles
            break;
        }

        cout << "Incorrect password. Try again or type 'cancel' to exit.\n";
}

    userRoles = db.getUserRoles(username, password); // Refresh roles with valid password

    if (userRoles.size() > 1) {
        cout << "\nYou have multiple roles:\n";
        for (size_t i = 0; i < userRoles.size(); ++i) {
            cout << i + 1 << ". " << userRoles[i].role << "\n";
        }
        cout << userRoles.size() + 1 << ". Delete ALL roles\n";
        cout << userRoles.size() + 2 << ". Cancel\n";

        int choice;
        cout << "Choose option: ";
        cin >> choice;

        if (choice == static_cast<int>(userRoles.size() + 2)) return;

        if (choice == static_cast<int>(userRoles.size() + 1)) {
            bool success = true;
            for (const auto& ur : userRoles) {
                if (!db.deleteUserAccount(ur.id, ur.role)) {
                    success = false;
                }
            }
            cout << (success ? "All roles deleted.\n" : "Error deleting roles.\n");
        } else if (choice > 0 && choice <= static_cast<int>(userRoles.size())) {
            const auto& roleToDelete = userRoles[choice - 1].role;
            if (db.deleteUserAccount(userRoles[choice - 1].id, roleToDelete)) {
                cout << "Role '" << roleToDelete << "' deleted.\n";
            } else {
                cout << "Failed to delete the role.\n";
            }
        } else {
            cout << "Invalid choice. Returning to menu.\n";
        }

    } else {
        char confirm;
        cout << "Confirm delete your '" << userRoles[0].role << "' account? (y/n): ";
        cin >> confirm;
        if (confirm == 'y' || confirm == 'Y') {
            if (db.deleteUserAccount(userRoles[0].id, userRoles[0].role)) {
                cout << "Account deleted.\n";
            } else {
                cout << "Failed to delete account.\n";
            }
        }
    }
}


// Main application
// Main application class to run the quiz system
class QuizApplication {
private:
    DatabaseManager db;

public:
    QuizApplication(const string& server, const string& user,
                   const string& password, const string& database)
        : db(server, user, password, database) {}

    void run() {
    while (true) {
        cout << "\nWelcome to LINQUIZ !!!!!\n";
        cout << "1. Login\n";
        cout << "2. Register\n";
        cout << "3. Exit\n";
        cout << "4. Delete My Account\n";
        cout << "Enter your choice: ";

        int choice;
        cin >> choice;
        cin.ignore(); // Clear newline

        switch (choice) {

    case 1: {
    string username, password;
    cout << "Username: ";
    getline(cin, username);
    password = getHiddenInput("Password: ");

    // Step 1: Verify password first
    if (!db.verifyPassword(username, password)) {
        cout << "\nInvalid username or password.\n";
        break;
    }

    // Step 2: Get all available roles
    auto allRoles = db.getAllRolesForUser(username);
    if (allRoles.empty()) {
        cout << "\nNo roles found for this user.\n";
        break;
    }

    unique_ptr<User> user;
    if (allRoles.size() == 1) {
        // Single role - auto login
        if (allRoles[0].role == "admin") {
            user = make_unique<Admin>(allRoles[0].id, username, password);
        } else {
            user = make_unique<Student>(allRoles[0].id, username, password);
        }
    } else {
        // Multiple roles - show selection
        cout << "\nMultiple roles available:\n";
        for (size_t i = 0; i < allRoles.size(); ++i) {
            cout << i+1 << ". Login as " << allRoles[i].role << "\n";
        }

        int choice;
        while (true) {
            cout << "Select role (1-" << allRoles.size() << "): ";
            cin >> choice;
            cin.ignore();

            if (choice > 0 && choice <= static_cast<int>(allRoles.size())) {
                break;
            }
            cout << "Invalid choice. Try again.\n";
        }

        if (allRoles[choice-1].role == "admin") {
            user = make_unique<Admin>(allRoles[choice-1].id, username, password);
        } else {
            user = make_unique<Student>(allRoles[choice-1].id, username, password);
        }
    }

    cout << "\nLogin successful! Welcome, " << user->getUsername()
         << " (" << user->getRole() << ").\n";
    user->displayMenu(db);
    break;
}

    case 2: {
    string username, password, confirmPassword, role;
    cout << "Username: ";
    getline(cin, username);

    // Get password with confirmation
    while (true) {
        password = getHiddenInput("Password: ");
        confirmPassword = getHiddenInput("Confirm Password: ");

        if (password == confirmPassword) {
            break;
        } else {
            cout << "\nPasswords do not match. Please try again.\n";
        }
    }

    // Role selection menu
    while (true) {
        cout << "\nSelect your role:\n";
        cout << "1. Student\n";
        cout << "2. Admin\n";
        cout << "Enter your choice (1-2): ";

        int roleChoice;
        cin >> roleChoice;
        cin.ignore();

        if (roleChoice == 1) {
            role = "student";
            break;
        } else if (roleChoice == 2) {
            role = "admin";
            break;
        } else {
            cout << "Invalid choice. Please try again.\n";
        }
    }

    if (db.registerUser(username, password, role)) {
        cout << "\nRegistration successful! Please login.\n";
    } else {
        cout << "\nRegistration failed (username already exists for this role).\n";
        cout << "Note: You can register the same username for different roles.\n";
    }
    break;
}

            case 3:{
                cout << "Goodbye!\n";
                return;
                break;
            }

            case 4 :
                deleteAccountFlow(db);
                break;
            default:
                cout << "Invalid choice. Try again.\n";
        }
    }
}
};

int main() {
    // Initialize MySQL connection parameters
    string server = "localhost";
    string user = "quiz_user";
    string password = "quiz_password";
    string database = "quiz_system";

    QuizApplication app(server, user, password, database);
    app.run();
    return 0;
}

