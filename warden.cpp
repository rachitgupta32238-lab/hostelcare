
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <limits>

#include "colors.h"
#include "warden.h"

using namespace std;


// =====================================================
//  Display complaints details
// =====================================================


void Complaint::display() const
{   
    
    cout << MAGENTA << "\n----------------------------------------\n"<<RESET ;
    cout << CYAN<<BOLD;
    cout << "Complaint ID : " << getComplaintId() << endl;
    cout << "Student ID   : " << getStudentId() << endl;
    cout << "Student Name : " << getStudentName() << endl;
    cout << "Room No      : " << getRoomNo() << endl;
    cout << "Hostel Name  : " << getHostelName() << endl;
    cout << "Category     : " << getCategory() << endl;
    cout << "Description  : " << getDescription() << endl;
    cout << "Priority     : " << getPriority() << endl;
    cout << "Status       : " << getStatus() << endl;
    cout << RESET;
    if (getWorkerId() == -1)
        cout <<RED<< "Worker ID    : Not Assigned\n" << RESET;
    else
        cout << CYAN << BOLD<<  "Worker ID    : " << getWorkerId() << endl;
    cout << CYAN << BOLD ;
    cout << "Feedback     : " << getFeedback() <<RESET<< endl;
    cout <<MAGENTA<< "----------------------------------------\n"<<RESET;
}

// =====================================================
// 1. load complaints
// =====================================================


void Warden::loadComplaints()
{
    complaints.clear();

    ifstream file(complaintFile);

    if (!file)
    {
        cout << RED << "\nNo complaint file found.\n" << RESET;
        return;
    }

    string line;

    while (getline(file, line))
    {
        if (line.empty())
            continue;

        stringstream ss(line);

        string complaintId;
        string studentId;
        string studentName;
        string roomNo;
        string hostelname;
        string category;
        string description;
        string priority;
        string status;
        string workerId;
        string feedback;

        getline(ss, complaintId, '|');
        getline(ss, studentId, '|');
        getline(ss, studentName, '|');
        getline(ss, roomNo, '|');
        getline(ss, hostelname, '|');
        getline(ss, category, '|');
        getline(ss, description, '|');
        getline(ss, priority, '|');
        getline(ss, status, '|');
        getline(ss, workerId, '|');
        getline(ss, feedback);

        try
        {
            Complaint c(
                stoi(complaintId),
                stoi(studentId),
                studentName,
                roomNo,
                hostelname,
                category,
                description,
                priority,
                status,
                stoi(workerId),
                feedback
            );

            complaints.push_back(c);
        }
        catch (...)
        {
            cout << RED
                 << "\nInvalid complaint record skipped.\n"
                 << RESET;
        }
    }

    file.close();
}


// =====================================================
// 2. save complaints
// =====================================================

void Warden::saveComplaints()
{
    ofstream file(complaintFile);

    if (!file)
    {
        cout << RED << "\nUnable to save complaints.\n" << RESET;
        return;
    }

    for (const Complaint& c : complaints)
    {
        file << c.getComplaintId() << "|"
             << c.getStudentId() << "|"
             << c.getStudentName() << "|"
             << c.getRoomNo() << "|"
             << c.getHostelName() << "|"
             << c.getCategory() << "|"
             << c.getDescription() << "|"
             << c.getPriority() << "|"
             << c.getStatus() << "|"
             << c.getWorkerId() << "|"
             << c.getFeedback()
             << "\n";
    }

    file.close();
}


// =====================================================
// 3. find complaint
// =====================================================

Complaint* Warden::findComplaint(int id)
{
    for (Complaint& c : complaints)
    {
        if (c.getComplaintId() == id)
            return &c;
    }

    return nullptr;
}


// =====================================================
// 4. get complaint id
// =====================================================

int Warden::getComplaintId()
{
    int id;

    cout << MAGENTA << "Enter Complaint ID: " << RESET;
    cin >> id;

    if (cin.fail())
    {
        cin.clear();

        cin.ignore(
            numeric_limits<streamsize>::max(),
            '\n'
        );

        return -1;
    }

    return id;
}


// =====================================================
// 5. view pending compaints
// =====================================================

void Warden::viewPendingComplaints()
{
    ifstream file(complaintFile);

    if (!file)
    {
        cout << RED << "\nNo complaint file found.\n" << RESET;
        return;
    }

    string line;
    bool found = false;

    cout << CYAN
         << "\n========== PENDING COMPLAINTS ==========\n"
         << RESET;

    while (getline(file, line))
    {
        if (line.empty())
            continue;

        stringstream ss(line);

        string complaintId, studentId, studentName;
        string roomNo, hostelname, category, description;
        string priority, status, workerId, feedback;

        getline(ss, complaintId, '|');
        getline(ss, studentId, '|');
        getline(ss, studentName, '|');
        getline(ss, roomNo, '|');
        getline(ss, hostelname, '|');
        getline(ss, category, '|');
        getline(ss, description, '|');
        getline(ss, priority, '|');
        getline(ss, status, '|');
        getline(ss, workerId, '|');
        getline(ss, feedback);

        if (status == "Pending")
        {
            cout << BLUE << BOLD;

            cout << "\n----------------------------------------\n";
            cout << "Complaint ID : " << complaintId << endl;
            cout << "Student ID   : " << studentId << endl;
            cout << "Student Name : " << studentName << endl;
            cout << "Room No      : " << roomNo << endl;
            cout << "Hostel Name  : " << hostelname << endl;
            cout << "Category     : " << category << endl;
            cout << "Description  : " << description << endl;
            cout << "Priority     : " << priority << endl;
            cout << "Status       : " << status << endl;

            cout << RESET << CYAN;

            if (workerId == "-1")
                cout << RED << "Worker ID    : Not Assigned\n" << RESET;
            else
                cout << "Worker ID    : " << workerId << endl;

            if (!feedback.empty())
                cout << "Feedback     : " << feedback << endl;

            cout << "----------------------------------------\n";
            cout << RESET;

            found = true;
        }
    }

    if (!found)
        cout << GREEN << "\nNo pending complaints found.\n" << RESET;

    file.close();
}


// =====================================================
// 6. verify complaint
// =====================================================

void Warden::verifyComplaint()
{
    loadComplaints();

    cout << CYAN
         << "\n========== VERIFY COMPLAINT ==========\n"
         << RESET;

    int id = getComplaintId();

    if (id == -1)
    {
        cout << RED << "\nInvalid Complaint ID.\n" << RESET;
        return;
    }

    Complaint* complaint = findComplaint(id);

    if (complaint == nullptr)
    {
        cout << RED << "\nComplaint not found.\n" << RESET;
        return;
    }

    if (complaint->getStatus() != "Pending")
    {
        cout << GREEN << "\nComplaint is not pending.\n" << RESET;

        cout << CYAN << "Current Status: "
             << complaint->getStatus()
             << RESET << endl;

        return;
    }

    complaint->setStatus("Verified");

    saveComplaints();

    cout << GREEN << "\nComplaint verified successfully.\n" << RESET;
}


// =====================================================
// 7. approve complaint
// =====================================================

void Warden::approveComplaint()
{
    loadComplaints();

    cout << CYAN
         << "\n========== APPROVE COMPLAINT ==========\n"
         << RESET;

    int id = getComplaintId();

    if (id == -1)
    {
        cout << RED << "\nInvalid Complaint ID.\n" << RESET;
        return;
    }

    Complaint* complaint = findComplaint(id);

    if (complaint == nullptr)
    {
        cout << RED << "\nComplaint not found.\n" << RESET;
        return;
    }

    if (complaint->getStatus() != "Verified")
    {
        cout << RED
             << "\nComplaint must be verified first.\n"
             << RESET;

        cout << CYAN << "Current Status: "
             << complaint->getStatus()
             << RESET << endl;

        return;
    }

    complaint->setStatus("Approved");

    saveComplaints();

    cout << GREEN << "\nComplaint approved successfully.\n" << RESET;
}


// =====================================================
// 8. reject complaint
// =====================================================

void Warden::rejectComplaint()
{
    loadComplaints();

    cout << CYAN
         << "\n========== REJECT COMPLAINT ==========\n"
         << RESET;

    int id = getComplaintId();

    if (id == -1)
    {
        cout << RED << "\nInvalid Complaint ID.\n" << RESET;
        return;
    }

    Complaint* complaint = findComplaint(id);

    if (complaint == nullptr)
    {
        cout << RED << "\nComplaint not found.\n" << RESET;
        return;
    }

    if (complaint->getStatus() == "Resolved")
    {
        cout << RED
             << "\nResolved complaint cannot be rejected.\n"
             << RESET;
        return;
    }

    complaint->setStatus("Rejected");

    saveComplaints();

    cout << GREEN << "\nComplaint rejected successfully.\n" << RESET;
}


// =====================================================
// 9. mark similar / duplicate complaint
// =====================================================

void Warden::markDuplicateComplaint()
{
    loadComplaints();

    cout << CYAN
         << "\n========== MARK SIMILAR COMPLAINT ==========\n"
         << RESET;

    int id = getComplaintId();

    if (id == -1)
    {
        cout << RED << "\nInvalid Complaint ID.\n" << RESET;
        return;
    }

    Complaint* complaint = findComplaint(id);

    if (complaint == nullptr)
    {
        cout << RED << "\nComplaint not found.\n" << RESET;
        return;
    }

    complaint->setStatus("Duplicate");

    saveComplaints();

    cout << GREEN
         << "\nComplaint marked as Similar Complaint.\n"
         << RESET;
}


// =====================================================
// 10. forward complaint
// =====================================================

void Warden::forwardComplaint()
{
    loadComplaints();

    cout << CYAN
         << "\n========== FORWARD COMPLAINT ==========\n"
         << RESET;

    int id = getComplaintId();

    if (id == -1)
    {
        cout << RED << "\nInvalid Complaint ID.\n" << RESET;
        return;
    }

    Complaint* complaint = findComplaint(id);

    if (complaint == nullptr)
    {
        cout << RED << "\nComplaint not found.\n" << RESET;
        return;
    }

    if (complaint->getStatus() != "Approved")
    {
        cout << RED
             << "\nOnly approved complaints can be forwarded.\n"
             << RESET;

        cout << CYAN << "Current Status: "
             << complaint->getStatus()
             << RESET << endl;

        return;
    }

    complaint->setStatus("Forwarded");

    saveComplaints();

    cout << GREEN
         << "\nComplaint forwarded to department.\n"
         << RESET;
}


// =====================================================
// 11. change final priority
// =====================================================

void Warden::changeFinalPriority()
{
    loadComplaints();

    cout << CYAN
         << "\n========== CHANGE FINAL PRIORITY ==========\n"
         << RESET;

    int id = getComplaintId();

    if (id == -1)
    {
        cout << RED << "\nInvalid Complaint ID.\n" << RESET;
        return;
    }

    Complaint* complaint = findComplaint(id);

    if (complaint == nullptr)
    {
        cout << RED << "\nComplaint not found.\n" << RESET;
        return;
    }

    cout << CYAN << "\nCurrent Priority: "
         << complaint->getPriority()
         << RESET << endl;

    cout << YELLOW;
    cout << "\n1. Low\n";
    cout << "2. Medium\n";
    cout << "3. High\n";
    cout << "4. Critical\n";
    cout << RESET;

    cout << MAGENTA << "Enter choice: " << RESET;

    int choice;
    cin >> choice;

    if (cin.fail())
    {
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');

        cout << RED << "\nInvalid choice.\n" << RESET;
        return;
    }

    switch (choice)
    {
        case 1:
            complaint->setPriority("Low");
            break;

        case 2:
            complaint->setPriority("Medium");
            break;

        case 3:
            complaint->setPriority("High");
            break;

        case 4:
            complaint->setPriority("Critical");
            break;

        default:
            cout << RED << "\nInvalid choice.\n" << RESET;
            return;
    }

    saveComplaints();

    cout << GREEN << "\nPriority changed successfully.\n" << RESET;

    cout << CYAN << "New Priority: " << RESET
         << YELLOW << complaint->getPriority()
         << RESET << endl;
}


// =====================================================
// 12. generate reports
// =====================================================

void Warden::generateReports()
{
    loadComplaints();

    int pending = 0;
    int verified = 0;
    int approved = 0;
    int rejected = 0;
    int duplicate = 0;
    int forwarded = 0;
    int resolved = 0;

    for (const Complaint& c : complaints)
    {
        if (c.getStatus() == "Pending")
            pending++;

        else if (c.getStatus() == "Verified")
            verified++;

        else if (c.getStatus() == "Approved")
            approved++;

        else if (c.getStatus() == "Rejected")
            rejected++;

        else if (c.getStatus() == "Duplicate")
            duplicate++;

        else if (c.getStatus() == "Forwarded")
            forwarded++;

        else if (c.getStatus() == "Resolved")
            resolved++;
    }

    cout << BLUE << BOLD;

    cout << "\n============================================\n";
    cout << "           HOSTELCARE REPORT\n";
    cout << "============================================\n";

    cout << GREEN << "\nTotal Complaints : " << RESET
         << CYAN << BOLD << complaints.size() << endl;

    cout << "\n----- STATUS REPORT -----\n";

    cout << "Pending          : " << pending << endl;
    cout << "Verified         : " << verified << endl;
    cout << "Approved         : " << approved << endl;
    cout << "Rejected         : " << rejected << endl;
    cout << "Duplicate        : " << duplicate << endl;
    cout << "Forwarded        : " << forwarded << endl;
    cout << "Resolved         : " << resolved << endl;

    cout << RESET;
}


// =====================================================
// 13. display all complaints with student details
// =====================================================

void Warden::displayAllComplaints()
{
    loadComplaints();

    if (complaints.empty())
    {
        cout << RED << "\nNo complaints found.\n" << RESET;
        return;
    }

    cout << YELLOW
         << "\n========== ALL COMPLAINT DETAILS ==========\n"
         << RESET;

    for (const Complaint& c : complaints)
    {
        c.display();
    }
}

// =====================================================
// 14. WARDEN MENU
// =====================================================

void Warden::menu()
{
    int choice;

    do
    {
        cout << BLUE << BOLD;

        cout << "\n========== WARDEN PORTAL ==========\n";
        cout << "1. View Pending Complaints\n";
        cout << "2. Verify Complaint\n";
        cout << "3. Approve Complaint\n";
        cout << "4. Reject Complaint\n";
        cout << "5. Similar Complaint\n";
        cout << "6. Forward Complaint to Department\n";
        cout << "7. Change Final Priority\n";
        cout << "8. Generate Reports\n";
        cout << "9. Display All Complaint Details\n";
        cout << "0. Back to Main Menu\n";

        cout << RESET;
        cout << MAGENTA << "Enter your choice: " << RESET;

        cin >> choice;

        if (cin.fail())
        {
            cin.clear();

            cin.ignore(
                numeric_limits<streamsize>::max(),
                '\n'
            );

            cout << RED << "\nInvalid input.\n" << RESET;
            continue;
        }

        switch (choice)
        {
            case 1:
                viewPendingComplaints();
                break;

            case 2:
                verifyComplaint();
                break;

            case 3:
                approveComplaint();
                break;

            case 4:
                rejectComplaint();
                break;

            case 5:
                markDuplicateComplaint();
                break;

            case 6:
                forwardComplaint();
                break;

            case 7:
                changeFinalPriority();
                break;

            case 8:
                generateReports();
                break;
            case 9:
                displayAllComplaints();
                break;
            case 0:
                cout << GREEN
                     << "\nReturning to Main Menu...\n"
                     << RESET;
                break;

            default:
                cout << RED
                     << "\nInvalid choice. Try again.\n"
                     << RESET;
        }

    } while (choice != 0);
}


// =====================================================
// GLOBAL WARDEN MENU FUNCTION
// =====================================================

void wardenMenu()
{
    Warden warden;
    warden.menu();
}
