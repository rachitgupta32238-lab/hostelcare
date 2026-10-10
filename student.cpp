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


//========temp0=========//


void StudentModule::registerComplaint()
{
    cout << "Complaint registration not implemented yet.\n";
}

void StudentModule::viewMyComplaints()
{
    cout << "View complaints not implemented yet.\n";
}

void StudentModule::trackComplaintStatus()
{
    cout << "Complaint tracking not implemented yet.\n";
}

void StudentModule::viewComplaintHistory()
{
    cout << "Complaint history not implemented yet.\n";
}

void StudentModule::giveFeedback()
{
    cout << "Feedback not implemented yet.\n";
}

