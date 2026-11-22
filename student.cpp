#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <limits>

#include "colors.h"
#include "student.h"

//total fields in student module
/*complaintId|studentId|studentName|roomNo|hostelName|
    category|description|priority|status|workerId|feedback*/

using namespace std;

void StudentModule::studentMenu() {
    int choice;

    do {
        cout << BLUE << BOLD;
        cout << "\n========== STUDENT PORTAL ==========\n";
        cout << "1. Register Student\n";
        cout << "2. Register Complaint\n";
        cout << "3. View My Complaints\n";
        cout << "4. Track Complaint Status\n";
        cout << "5. View Complaint History\n";
        cout << "6. Give Feedback\n";
        cout << "0. Back to Main Menu\n";
        cout << RESET;
        cout << MAGENTA << "Enter your choice: " << RESET;


        cin >> choice;

        if (cin.fail()) {
            cin.clear();
            cout << RED << "Invalid input. Please enter a number.\n" << RESET;
            continue;
        }

        
        switch (choice) {
                    case 1:
                        registerStudent();
                        break;

                    case 2:
                        registerComplaint();
                        break;

                    case 3:
                        viewMyComplaints();
                        break;

                    case 4:
                        trackComplaintStatus();
                        break;

                    case 5:
                        viewComplaintHistory();
                        break;

                    case 6:
                        giveFeedback();
                        break;

                    case 0:
                        cout << GREEN << "\nReturning to main menu...\n" << RESET;
                        break;

                    default:
                        cout << RED << "\nInvalid choice. Try again.\n" << RESET;
                }
    } while (choice != 0);
}


//========================================//
//function defination//


void StudentModule::registerStudent()
{
    Student s;

    cout << CYAN << BOLD;
    cout << "\n========== STUDENT REGISTRATION ==========\n";

    cout << "Enter Student ID: ";
    cin >> s.studentId;

    if (cin.fail())
    {
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        cout << RED << "Invalid Student ID.\n" << RESET;
        return;
    }

    // Check whether this Student ID already exists
    Student existing;

    if (findStudent(s.studentId, existing))
    {
        cout << YELLOW << "Student ID already registered.\n" << RESET;
        return;
    }

    cin.ignore(numeric_limits<streamsize>::max(), '\n');

    cout << "Enter Name: ";
    getline(cin, s.name);

    cout << "Enter Room Number: ";
    getline(cin, s.roomNo);

    cout << "Enter Contact Number: ";
    getline(cin, s.contact);

    cout << "Enter hostel name: ";
    getline(cin, s.hostelName);

    ofstream file(STUDENT_FILE, ios::app);

    if (!file)
    {
        cout << RED << "Unable to open student file.\n" << RESET;
        cout << RED << "Ensure the data folder exists.\n" << RESET;
        return;
    }

    file << s.studentId << "|"
         << s.name << "|"
         << s.roomNo << "|"
         << s.contact << "|"
         << s.hostelName << "\n";

    file.close();

    cout <<  GREEN << "\nStudent registered successfully!\n" << RESET;
    cout << RESET;
}


//==============================//


bool StudentModule::findStudent(int id, Student &student)
{
    ifstream file(STUDENT_FILE);

    if (!file)
        return false;

    string line;

    while (getline(file, line))
    {
        if (line.empty())
            continue;

        stringstream ss(line);
        string studentId;

        Student s;

        getline(ss, studentId, '|');
        getline(ss, s.name, '|');
        getline(ss, s.roomNo, '|');
        getline(ss, s.contact, '|');
        getline(ss, s.hostelName, '|');

        try
        {
            s.studentId = stoi(studentId);
        }
        catch (...)
        {
            continue;
        }

        if (s.studentId == id)
        {
            student = s;
            file.close();
            return true;
        }
    }

    file.close();
    return false;
}



int StudentModule::generateComplaintId()
{
    ifstream file(COMPLAINT_FILE);

    int maxId = 0;
    string line;

    while (getline(file, line))
    {
        if (line.empty())
            continue;

        stringstream ss(line);
        string id;

        getline(ss, id, '|');

        try
        {
            int currentId = stoi(id);

            if (currentId > maxId)
                maxId = currentId;
        }
        catch (...)
        {
            continue;
        }
    }

    file.close();

    return maxId + 1;
}



void StudentModule::registerComplaint()
{
    Student s;
    Complaint c;

    cout << CYAN << BOLD;
    cout << "\n========== REGISTER COMPLAINT ==========\n";

    cout << "Enter Student ID: ";
    cin >> s.studentId;

    if (cin.fail())
    {
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        cout << RED << "Invalid Student ID.\n" << RESET;
        return;
    }

    // Find registered student
    if (!findStudent(s.studentId, s))
    {
        cout << RED <<  "Student not registered. Please register first.\n" << RESET;
        return;
    }

    c.studentId = s.studentId;
    c.studentName = s.name;
    c.roomNo = s.roomNo;
    c.hostelName = s.hostelName;

    cin.ignore(numeric_limits<streamsize>::max(), '\n');

    cout << "Enter complaint category: ";
    getline(cin, c.category);

    cout << "Enter complaint description: ";
    getline(cin, c.description);

    c.complaintId = generateComplaintId();
    c.priority = "Medium";
    c.status = "Pending";
    c.workerId = -1;
    c.feedback = "";

    ofstream file(COMPLAINT_FILE, ios::app);

    if (!file)
    {
        cout << RED << "Unable to open complaint file.\n" << RESET;
        cout << RED << "Ensure the data folder exists.\n" << RESET;
        return;
    }

    file << c.complaintId << "|"
         << c.studentId << "|"
         << c.studentName << "|"
         << c.roomNo << "|"
         << c.hostelName << "|"
         << c.category << "|"
         << c.description << "|"
         << c.priority << "|"
         << c.status << "|"
         << c.workerId << "|"
         << c.feedback << "\n";

    file.close();

    cout << GREEN << "\nComplaint registered successfully!\n" << RESET;
    cout << "Complaint ID: " << c.complaintId << "\n";
    cout << "Initial Status: " << c.status << "\n";
    cout << RESET;
}



void StudentModule::viewMyComplaints()
{
    int id;

    cout << CYAN << BOLD;
    cout << "\n========== MY COMPLAINTS ==========\n";
    cout << "Enter Student ID: ";
    cin >> id;

    if (cin.fail())
    {
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        cout << RED << "Invalid Student ID.\n" << RESET;
        return;
    }

    Student s;

    if (!findStudent(id, s))
    {
        cout << RED << "Student not registered.\n" << RESET;
        return;
    }

    ifstream file(COMPLAINT_FILE);

    if (!file)
    {
        cout << RED << "Unable to open complaint file.\n" << RESET ;
        return;
    }

    string line;
    bool found = false;

    while (getline(file, line))
    {
        if (line.empty())
            continue;

        stringstream ss(line);
        string fields[11];

        for (int i = 0; i < 11; i++)
            getline(ss, fields[i], '|');

        try
        {
            if (stoi(fields[1]) != id)
                continue;
        }
        catch (...)
        {
            continue;
        }

        found = true;

        cout << YELLOW << BOLD;
        cout << "\nComplaint ID : " << fields[0] << '\n';
        cout << "Category     : " << fields[5] << '\n';
        cout << "Description  : " << fields[6] << '\n';
        cout << "Priority     : " << fields[7] << '\n';
        cout << "Status       : " << fields[8] << '\n';
        cout << "Worker ID    : " << fields[9] << '\n';
        cout << "-------------------------------\n";
        cout << RESET;

    }

    file.close();

    if (!found)
        cout << RED << "No complaints found for this student.\n" << RESET;
    
    cout << RESET;
}




void StudentModule::trackComplaintStatus()
{
    int studentId, complaintId;

    cout << CYAN << BOLD;
    cout << "\n========== TRACK COMPLAINT ==========\n";

    cout << "Enter Student ID: ";
    cin >> studentId;

    if (cin.fail())
    {
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        cout << RED << "Invalid Student ID.\n" << RESET;
        return;
    }

    // Verify that the student is registered
    Student s;

    if (!findStudent(studentId, s))
    {
        cout << RED << "Student not registered.\n" << RESET;
        return;
    }

    cout << "Enter Complaint ID: ";
    cin >> complaintId;

    if (cin.fail())
    {
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        cout << RED << "Invalid Complaint ID.\n" << RESET;
        return;
    }

    ifstream file(COMPLAINT_FILE);

    if (!file)
    {
        cout << RED << "Unable to open complaint file.\n" << RESET;
        return;
    }

    string line;
    bool found = false;

    while (getline(file, line))
    {
        if (line.empty())
            continue;

        stringstream ss(line);
        string fields[11];

        for (int i = 0; i < 11; i++)
            getline(ss, fields[i], '|');

        try
        {
            int storedComplaintId = stoi(fields[0]);
            int storedStudentId = stoi(fields[1]);

            // Match both IDs
            if (storedComplaintId != complaintId ||
                storedStudentId != studentId)
            {
                continue;
            }
        }
        catch (...)
        {
            continue;
        }

        cout << BLUE << BOLD;
        cout << "\nComplaint ID  : " << fields[0] << '\n';
        cout << "Category      : " << fields[5] << '\n';
        cout << "Description   : " << fields[6] << '\n';
        cout << "Priority      : " << fields[7] << '\n';
        cout << "Current Status: " << fields[8] << '\n';

        if (fields[9] == "-1")
            cout << "Worker        : Not assigned yet\n";
        else
            cout << "Worker ID     : " << fields[9] << '\n';

        found = true;
        cout << RESET;
        break;
    }

    file.close();

    if (!found)
    {
        cout << RED << "Complaint not found for this Student ID.\n" << RESET;
    }

    cout << RESET;
}



void StudentModule::viewComplaintHistory()
{
    int studentId;

    cout << CYAN << BOLD;
    cout << "\n========== COMPLAINT HISTORY ==========\n";
    cout << "Enter Student ID: ";
    cin >> studentId;

    if (cin.fail())
    {
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        cout << RED << "Invalid Student ID.\n" << RESET;
        return;
    }

    Student s;

    if (!findStudent(studentId, s))
    {
        cout << RED << "Student not registered.\n" << RESET;
        return;
    }

    ifstream file(COMPLAINT_FILE);

    if (!file)
    {
        cout << RED << "Unable to open complaint file.\n" << RESET;
        return;
    }

    string line;
    bool found = false;

    cout << YELLOW << "\nStudent: " << s.name << '\n' << RESET;

    while (getline(file, line))
    {
        if (line.empty())
            continue;

        stringstream ss(line);
        string fields[11];

        for (int i = 0; i < 11; i++)
            getline(ss, fields[i], '|');

        try
        {
            if (stoi(fields[1]) != studentId)
                continue;
        }
        catch (...)
        {
            continue;
        }

        found = true;
        cout << BLUE << BOLD;
        cout << "\nComplaint ID : " << fields[0] << '\n';
        cout << "Category     : " << fields[5] << '\n';
        cout << "Description  : " << fields[6] << '\n';
        cout << "Priority     : " << fields[7] << '\n';
        cout << "Status       : " << fields[8] << '\n';

        if (fields[9] == "-1")
            cout << "Worker       : Not assigned\n";
        else
            cout << "Worker ID    : " << fields[9] << '\n';

        cout << "-------------------------------\n";
        cout << RESET;
    }

    file.close();

    if (!found)
        cout << RED << "No complaint history found.\n" << RESET;

    cout << RESET;
}



void StudentModule::giveFeedback()
{
    int studentId, complaintId;

    cout << CYAN << BOLD;
    cout << "\n========== GIVE FEEDBACK ==========\n";

    cout << "Enter Student ID: ";
    cin >> studentId;

    if (cin.fail())
    {
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        cout << RED << "Invalid Student ID.\n" << RESET;
        return;
    }

    Student s;

    if (!findStudent(studentId, s))
    {
        cout << RED << "Student not registered.\n" << RESET;
        return;
    }

    cout << "Enter Complaint ID: ";
    cin >> complaintId;

    if (cin.fail())
    {
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        cout << "Invalid Complaint ID.\n";
        return;
    }

    // Load complaint records
    ifstream file(COMPLAINT_FILE);

    if (!file)
    {
        cout << RED << "Unable to open complaint file.\n" << RESET;
        return;
    }

    string lines[1000];
    int count = 0;
    int targetIndex = -1;
    string line;

    while (getline(file, line) && count < 1000)
    {
        stringstream ss(line);
        string fields[11];

        for (int i = 0; i < 11; i++)
            getline(ss, fields[i], '|');

        try
        {
            if (stoi(fields[0]) == complaintId &&
                stoi(fields[1]) == studentId)
            {
                if (fields[8] != "Resolved")
                {
                    cout << YELLOW << "Feedback is allowed only for resolved complaints.\n" << RESET;
                    file.close();
                    return;
                }

                if (!fields[10].empty())
                {
                    cout << GREEN << "Feedback has already been submitted.\n" << RESET;
                    file.close();
                    return;
                }

                targetIndex = count;
            }
        }
        catch (...)
        {
            // Keep the record unchanged if it cannot be parsed.
        }

        lines[count++] = line;
    }

    file.close();

    if (targetIndex == -1)
    {
        cout << YELLOW << "Complaint not found for this Student ID.\n" << RESET;
        return;
    }

    cin.ignore(numeric_limits<streamsize>::max(), '\n');

    string feedback;
    cout << "Enter your feedback: ";
    getline(cin, feedback);

    if (feedback.empty() || feedback.find('|') != string::npos)
    {
        cout << RED << "Feedback cannot be empty or contain '|'.\n" << RESET;
        return;
    }

    stringstream ss(lines[targetIndex]);
    string fields[11];

    for (int i = 0; i < 11; i++)
        getline(ss, fields[i], '|');

    fields[10] = feedback;

    stringstream updated;
    for (int i = 0; i < 11; i++)
    {
        if (i > 0)
            updated << '|';
        updated << fields[i];
    }

    lines[targetIndex] = updated.str();

    ofstream out(COMPLAINT_FILE);

    if (!out)
    {
        cout << RED << "Unable to save feedback.\n" << RESET;
        return;
    }

    for (int i = 0; i < count; i++)
        out << lines[i] << '\n';

    out.close();

    cout << GREEN << "Feedback submitted successfully!\n" << RESET;

    cout << RESET;
}
