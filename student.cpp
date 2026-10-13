#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <limits>

#include "student.h"

using namespace std;

void StudentModule::studentMenu() {
    int choice;

    do {
        
        cout << "\n========== STUDENT PORTAL ==========\n";
        cout << "1. Register Student\n";
        cout << "2. Register Complaint\n";
        cout << "3. View My Complaints\n";
        cout << "4. Track Complaint Status\n";
        cout << "5. View Complaint History\n";
        cout << "6. Give Feedback\n";
        cout << "0. Back to Main Menu\n";
        cout << "Enter your choice: ";


        cin >> choice;

        if (cin.fail()) {
            cin.clear();
            cout << "Invalid input. Please enter a number.\n";
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
                        cout << "\nReturning to main menu...\n";
                        break;

                    default:
                        cout << "\nInvalid choice. Try again.\n";
                }
    } while (choice != 0);
}


//========================================//
//function defination//


void StudentModule::registerStudent()
{
    Student s;

    cout << "\n========== STUDENT REGISTRATION ==========\n";

    cout << "Enter Student ID: ";
    cin >> s.studentId;

    if (cin.fail())
    {
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        cout << "Invalid Student ID.\n";
        return;
    }

    // Check whether this Student ID already exists
    Student existing;

    if (findStudent(s.studentId, existing))
    {
        cout << "Student ID already registered.\n";
        return;
    }

    cin.ignore(numeric_limits<streamsize>::max(), '\n');

    cout << "Enter Name: ";
    getline(cin, s.name);

    cout << "Enter Room Number: ";
    getline(cin, s.roomNo);

    cout << "Enter Contact Number: ";
    getline(cin, s.contact);

    ofstream file(STUDENT_FILE, ios::app);

    if (!file)
    {
        cout << "Unable to open student file.\n";
        cout << "Ensure the data folder exists.\n";
        return;
    }

    file << s.studentId << "|"
         << s.name << "|"
         << s.roomNo << "|"
         << s.contact << "\n";

    file.close();

    cout << "\nStudent registered successfully!\n";
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
        getline(ss, s.contact);

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

    cout << "\n========== REGISTER COMPLAINT ==========\n";

    cout << "Enter Student ID: ";
    cin >> s.studentId;

    if (cin.fail())
    {
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        cout << "Invalid Student ID.\n";
        return;
    }

    // Find registered student
    if (!findStudent(s.studentId, s))
    {
        cout << "Student not registered. Please register first.\n";
        return;
    }

    c.studentId = s.studentId;
    c.studentName = s.name;
    c.roomNo = s.roomNo;

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
        cout << "Unable to open complaint file.\n";
        cout << "Ensure the data folder exists.\n";
        return;
    }

    file << c.complaintId << "|"
         << c.studentId << "|"
         << c.studentName << "|"
         << c.roomNo << "|"
         << c.category << "|"
         << c.description << "|"
         << c.priority << "|"
         << c.status << "|"
         << c.workerId << "|"
         << c.feedback << "\n";

    file.close();

    cout << "\nComplaint registered successfully!\n";
    cout << "Complaint ID: " << c.complaintId << "\n";
    cout << "Initial Status: " << c.status << "\n";
}



void StudentModule::viewMyComplaints()
{
    int id;

    cout << "\n========== MY COMPLAINTS ==========\n";
    cout << "Enter Student ID: ";
    cin >> id;

    if (cin.fail())
    {
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        cout << "Invalid Student ID.\n";
        return;
    }

    Student s;

    if (!findStudent(id, s))
    {
        cout << "Student not registered.\n";
        return;
    }

    ifstream file(COMPLAINT_FILE);

    if (!file)
    {
        cout << "Unable to open complaint file.\n";
        return;
    }

    string line;
    bool found = false;

    while (getline(file, line))
    {
        if (line.empty())
            continue;

        stringstream ss(line);
        string fields[10];

        for (int i = 0; i < 10; i++)
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

        cout << "\nComplaint ID : " << fields[0] << '\n';
        cout << "Category     : " << fields[4] << '\n';
        cout << "Description  : " << fields[5] << '\n';
        cout << "Priority     : " << fields[6] << '\n';
        cout << "Status       : " << fields[7] << '\n';
        cout << "Worker ID    : " << fields[8] << '\n';
        cout << "-------------------------------\n";
    }

    file.close();

    if (!found)
        cout << "No complaints found for this student.\n";
}




void StudentModule::trackComplaintStatus()
{
    int studentId, complaintId;

    cout << "\n========== TRACK COMPLAINT ==========\n";

    cout << "Enter Student ID: ";
    cin >> studentId;

    if (cin.fail())
    {
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        cout << "Invalid Student ID.\n";
        return;
    }

    // Verify that the student is registered
    Student s;

    if (!findStudent(studentId, s))
    {
        cout << "Student not registered.\n";
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

    ifstream file(COMPLAINT_FILE);

    if (!file)
    {
        cout << "Unable to open complaint file.\n";
        return;
    }

    string line;
    bool found = false;

    while (getline(file, line))
    {
        if (line.empty())
            continue;

        stringstream ss(line);
        string fields[10];

        for (int i = 0; i < 10; i++)
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

        cout << "\nComplaint ID  : " << fields[0] << '\n';
        cout << "Category      : " << fields[4] << '\n';
        cout << "Description   : " << fields[5] << '\n';
        cout << "Priority      : " << fields[6] << '\n';
        cout << "Current Status: " << fields[7] << '\n';

        if (fields[8] == "-1")
            cout << "Worker        : Not assigned yet\n";
        else
            cout << "Worker ID     : " << fields[8] << '\n';

        found = true;
        break;
    }

    file.close();

    if (!found)
    {
        cout << "Complaint not found for this Student ID.\n";
    }
}



void StudentModule::viewComplaintHistory()
{
    int studentId;

    cout << "\n========== COMPLAINT HISTORY ==========\n";
    cout << "Enter Student ID: ";
    cin >> studentId;

    if (cin.fail())
    {
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        cout << "Invalid Student ID.\n";
        return;
    }

    Student s;

    if (!findStudent(studentId, s))
    {
        cout << "Student not registered.\n";
        return;
    }

    ifstream file(COMPLAINT_FILE);

    if (!file)
    {
        cout << "Unable to open complaint file.\n";
        return;
    }

    string line;
    bool found = false;

    cout << "\nStudent: " << s.name << '\n';

    while (getline(file, line))
    {
        if (line.empty())
            continue;

        stringstream ss(line);
        string fields[10];

        for (int i = 0; i < 10; i++)
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

        cout << "\nComplaint ID : " << fields[0] << '\n';
        cout << "Category     : " << fields[4] << '\n';
        cout << "Description  : " << fields[5] << '\n';
        cout << "Priority     : " << fields[6] << '\n';
        cout << "Status       : " << fields[7] << '\n';

        if (fields[8] == "-1")
            cout << "Worker       : Not assigned\n";
        else
            cout << "Worker ID    : " << fields[8] << '\n';

        cout << "-------------------------------\n";
    }

    file.close();

    if (!found)
        cout << "No complaint history found.\n";
}



void StudentModule::giveFeedback()
{
    int studentId, complaintId;

    cout << "\n========== GIVE FEEDBACK ==========\n";

    cout << "Enter Student ID: ";
    cin >> studentId;

    if (cin.fail())
    {
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        cout << "Invalid Student ID.\n";
        return;
    }

    Student s;

    if (!findStudent(studentId, s))
    {
        cout << "Student not registered.\n";
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
        cout << "Unable to open complaint file.\n";
        return;
    }

    string lines[1000];
    int count = 0;
    int targetIndex = -1;
    string line;

    while (getline(file, line) && count < 1000)
    {
        stringstream ss(line);
        string fields[10];

        for (int i = 0; i < 10; i++)
            getline(ss, fields[i], '|');

        try
        {
            if (stoi(fields[0]) == complaintId &&
                stoi(fields[1]) == studentId)
            {
                if (fields[7] != "Resolved")
                {
                    cout << "Feedback is allowed only for resolved complaints.\n";
                    file.close();
                    return;
                }

                if (!fields[9].empty())
                {
                    cout << "Feedback has already been submitted.\n";
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
        cout << "Complaint not found for this Student ID.\n";
        return;
    }

    cin.ignore(numeric_limits<streamsize>::max(), '\n');

    string feedback;
    cout << "Enter your feedback: ";
    getline(cin, feedback);

    if (feedback.empty() || feedback.find('|') != string::npos)
    {
        cout << "Feedback cannot be empty or contain '|'.\n";
        return;
    }

    stringstream ss(lines[targetIndex]);
    string fields[10];

    for (int i = 0; i < 9; i++)
        getline(ss, fields[i], '|');

    fields[9] = feedback;

    stringstream updated;
    for (int i = 0; i < 10; i++)
    {
        if (i > 0)
            updated << '|';
        updated << fields[i];
    }

    lines[targetIndex] = updated.str();

    ofstream out(COMPLAINT_FILE);

    if (!out)
    {
        cout << "Unable to save feedback.\n";
        return;
    }

    for (int i = 0; i < count; i++)
        out << lines[i] << '\n';

    out.close();

    cout << "Feedback submitted successfully!\n";
}
