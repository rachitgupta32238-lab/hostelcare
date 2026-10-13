#include <iostream>
#include <limits>
#include <string>

#include"student.h"

#include"warden.h"

using namespace std;

void clearInputBuffer() {
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
}

void printHeader() {
    cout << "\n";
    cout << "============================================\n";
    cout << "       HOSTELCARE SYSTEM\n";
    cout << " Smart Complaint and Maintenance Platform\n";
    cout << "============================================\n";
}




void workerMenu() {
    int choice;

    do {
        cout << "\n========== WORKER PORTAL ==========\n";
        cout << "1. View Assigned Tasks\n";
        cout << "2. View Next Priority Task\n";
        cout << "3. Accept Task\n";
        cout << "4. Mark Task In Progress\n";
        cout << "5. Mark Task Resolved\n";
        cout << "6. Escalate Task\n";
        cout << "7. View Shortest Route to Complaint\n";
        cout << "0. Back to Main Menu\n";
        cout << "Enter your choice: ";

        cin >> choice;

        if (cin.fail()) {
            cin.clear();
            clearInputBuffer();
            cout << "Invalid input. Please enter a number.\n";
            continue;
        }

        switch (choice) {
            case 1:
                cout << "\n[Worker] Assigned tasks module coming soon.\n";
                break;

            case 2:
                cout << "\n[Worker] Priority queue module coming soon.\n";
                break;

            case 3:
                cout << "\n[Worker] Task acceptance module coming soon.\n";
                break;

            case 4:
                cout << "\n[Worker] Work status module coming soon.\n";
                break;

            case 5:
                cout << "\n[Worker] Task resolution module coming soon.\n";
                break;

            case 6:
                cout << "\n[Worker] Task escalation module coming soon.\n";
                break;

            case 7:
                cout << "\n[Worker] Dijkstra shortest-route module coming soon.\n";
                break;

            case 0:
                cout << "\nReturning to main menu...\n";
                break;

            default:
                cout << "\nInvalid choice. Try again.\n";
        }

    } while (choice != 0);
}

void aboutProject() {
    cout << "\n========== ABOUT HOSTELCARE ==========\n";
    cout << "HostelCare is a C++ and DSA-based hostel complaint system.\n";
    cout << "Students submit complaints for warden verification.\n";
    cout << "Approved complaints are sent to departments using priority queues.\n";
    cout << "Graphs and Dijkstra's algorithm help find the nearest suitable worker.\n";
}

int main() {
    int choice;
    StudentModule student;

    printHeader();

    do {
        cout << "\n========== MAIN MENU ==========\n";
        cout << "1. Student Portal\n";
        cout << "2. Warden Portal\n";
        cout << "3. Worker Portal\n";
        cout << "4. About Project\n";
        cout << "0. Exit\n";
        cout << "Enter your choice: ";

        cin >> choice;

        if (cin.fail()) {
            cin.clear();
            clearInputBuffer();
            cout << "\nInvalid input. Please enter a number.\n";
            continue;
        }

        switch (choice) {
            case 1:
                student.studentMenu();
                break;

            case 2:
                wardenMenu();
                break;

            case 3:
                workerMenu();
                break;

            case 4:
                aboutProject();
                break;

            case 0:
                cout << "\nThank you for using HostelCare.\n";
                break;

            default:
                cout << "\nInvalid choice. Try again.\n";
        }

    } while (choice != 0);

    return 0;
}
