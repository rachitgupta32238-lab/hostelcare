#include <iostream>
#include <limits>
#include <string>

#include "colors.h"
#include "student.h"
#include "warden.h"
#include "workerportal.h"
#include "campusgraph.h"

using namespace std;

void clearInputBuffer() {
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
}

void printHeader() {
    cout << CYAN << BOLD;
    cout << "\n";
    cout << "============================================\n";
    cout << "       HOSTELCARE SYSTEM\n";
    cout << " Smart Complaint and Maintenance Platform\n";
    cout << "============================================\n";
    cout << RESET;
}

void aboutProject() {
    cout << MAGENTA << BOLD;
    cout << "\n========== ABOUT HOSTELCARE ==========\n";
    cout << "HostelCare is a C++ and DSA-based hostel complaint system.\n";
    cout << "Students submit complaints for warden verification.\n";
    cout << "Approved complaints are sent to departments using priority handling.\n";
    cout << "Graphs and Dijkstra's algorithm help find suitable worker routes.\n";
    cout << RESET;
}

int main() {
    int choice = -1;
    StudentModule student;

    CampusGraph graph;
    graph.loadDefaultCampusMap();

    // Worker accounts and dispatch setup are encapsulated inside WorkerPortal.
    WorkerPortal workerPortal(graph);

    printHeader();

    do {
        cout << BLUE << BOLD;
        cout << "\n========== MAIN MENU ==========\n";
        cout << "1. Student Portal\n";
        cout << "2. Warden Portal\n";
        cout << "3. Worker Portal\n";
        cout << "4. About Project\n";
        cout << "0. Exit\n";
        cout << RESET;
        cout << MAGENTA << "Enter your choice: " << RESET;

        if (!(cin >> choice)) {
            cin.clear();
            clearInputBuffer();
            cout << RED << "\nInvalid input. Please enter a number.\n" << RESET;
            choice = -1;
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
                workerPortal.run();
                break;
            case 4:
                aboutProject();
                break;
            case 0:
                cout << GREEN << "\nThank you for using HostelCare.\n" << RESET;
                break;
            default:
                cout << RED << "Invalid choice. Try again.\n" << RESET;
        }
    } while (choice != 0);

    return 0;
}
