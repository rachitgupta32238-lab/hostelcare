#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <limits>

#include "student.h"

using namespace std;

// =====================================================
// STRUCTURES
// =====================================================

struct Student {

    int studentId;
    string name;
    string roomNo;
    string contact;
};

struct Complaint {

    int complaintId;
    int studentId;

    string studentName;
    string roomNo;

    string category;
    string description;

    string priority;
    string status;

    int workerId;

    string feedback;
};

// =====================================================
// FILE NAMES
// =====================================================

const string STUDENT_FILE = "data/students.txt";
const string COMPLAINT_FILE = "data/complaints.txt";

// =====================================================
// FUNCTION DECLARATIONS
// =====================================================

void registerStudent();
void registerComplaint();
void viewMyComplaints();
void trackComplaintStatus();
void viewComplaintHistory();
void giveFeedback();

bool findStudent(int id, Student &student);

int generateComplaintId();

vector<Complaint> loadComplaints();

void saveComplaint(const Complaint &complaint);

void updateComplaint(const Complaint &updated);

// =====================================================
// STUDENT MENU
// =====================================================

void studentMenu() {
    int choice;

    do {
        cout << "\n========== STUDENT PORTAL ==========\n";
        cout << "1. Register Complaint\n";
        cout << "2. View My Complaints\n";
        cout << "3. Track Complaint Status\n";
        cout << "4. View Complaint History\n";
        cout << "5. Give Feedback\n";
        cout << "0. Back to Main Menu\n";
        cout << "Enter your choice: ";

        cin >> choice;

        if (cin.fail()) {
            cin.clear();
            //clearInputBuffer();
            cout << "Invalid input. Please enter a number.\n";
            continue;
        }

        switch (choice) {
            case 1:
                cout << "\n[Student] Complaint registration module coming soon.\n";
                break;

            case 2:
                cout << "\n[Student] View complaints module coming soon.\n";
                break;

            case 3:
                cout << "\n[Student] Complaint tracking module coming soon.\n";
                break;

            case 4:
                cout << "\n[Student] Complaint history module coming soon.\n";
                break;

            case 5:
                cout << "\n[Student] Feedback module coming soon.\n";
                break;

            case 0:
                cout << "\nReturning to main menu...\n";
                break;

            default:
                cout << "\nInvalid choice. Try again.\n";
        }

    } while (choice != 0);
}