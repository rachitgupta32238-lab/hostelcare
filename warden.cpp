#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <limits>

#include "warden.h"
#include "viewPendingComplaints.h"
using namespace std;


// =====================================================
// COMPLAINT CLASS
// =====================================================

class Complaint
{
private:

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


public:

    // Constructor
    Complaint()
    {
        complaintId = 0;
        studentId = 0;
        workerId = -1;
    }


    // Parameterized Constructor
    Complaint(
        int complaintId,
        int studentId,
        string studentName,
        string roomNo,
        string category,
        string description,
        string priority,
        string status,
        int workerId,
        string feedback
    )
    {
        this->complaintId = complaintId;
        this->studentId = studentId;
        this->studentName = studentName;
        this->roomNo = roomNo;
        this->category = category;
        this->description = description;
        this->priority = priority;
        this->status = status;
        this->workerId = workerId;
        this->feedback = feedback;
    }


    // ================= GETTERS =================

    int getComplaintId() const
    {
        return complaintId;
    }

    int getStudentId() const
    {
        return studentId;
    }

    string getStudentName() const
    {
        return studentName;
    }

    string getRoomNo() const
    {
        return roomNo;
    }

    string getCategory() const
    {
        return category;
    }

    string getDescription() const
    {
        return description;
    }

    string getPriority() const
    {
        return priority;
    }

    string getStatus() const
    {
        return status;
    }

    int getWorkerId() const
    {
        return workerId;
    }

    string getFeedback() const
    {
        return feedback;
    }


    // ================= SETTERS =================

    void setStatus(const string& newStatus)
    {
        status = newStatus;
    }

    void setPriority(const string& newPriority)
    {
        priority = newPriority;
    }

    void setWorkerId(int id)
    {
        workerId = id;
    }


    // ================= DISPLAY =================

    void display() const
    {
        cout << "\n----------------------------------------\n";

        cout << "Complaint ID : " << complaintId << endl;
        cout << "Student ID   : " << studentId << endl;
        cout << "Student Name : " << studentName << endl;
        cout << "Room No      : " << roomNo << endl;
        cout << "Category     : " << category << endl;
        cout << "Description  : " << description << endl;
        cout << "Priority     : " << priority << endl;
        cout << "Status       : " << status << endl;

        if (workerId == -1)
            cout << "Worker ID    : Not Assigned" << endl;
        else
            cout << "Worker ID    : " << workerId << endl;

        if (!feedback.empty())
            cout << "Feedback     : " << feedback << endl;

        cout << "----------------------------------------\n";
    }
};


// =====================================================
// WARDEN CLASS
// =====================================================

class Warden
{
private:

    vector<Complaint> complaints;

    const string complaintFile = "data/complaints.txt";


    // ================= FILE OPERATIONS =================

    void loadComplaints()
    {
        complaints.clear();

        ifstream file(complaintFile);

        if (!file)
        {
            cout << "\nNo complaint file found.\n";
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
                cout << "\nInvalid complaint record skipped.\n";
            }
        }

        file.close();
    }


    void saveComplaints()
    {
        ofstream file(complaintFile);

        if (!file)
        {
            cout << "\nUnable to save complaints.\n";
            return;
        }


        for (const Complaint& c : complaints)
        {
            file << c.getComplaintId() << "|"
                 << c.getStudentId() << "|"
                 << c.getStudentName() << "|"
                 << c.getRoomNo() << "|"
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


    // ================= FIND COMPLAINT =================

    Complaint* findComplaint(int id)
    {
        for (Complaint& c : complaints)
        {
            if (c.getComplaintId() == id)
                return &c;
        }

        return nullptr;
    }


    // ================= INPUT =================

    int getComplaintId()
    {
        int id;

        cout << "Enter Complaint ID: ";
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


public:

    // =================================================
    // 1. VIEW PENDING COMPLAINTS
    // =================================================

    


    // =================================================
    // 2. VERIFY COMPLAINT
    // =================================================

    void verifyComplaint()
    {
        loadComplaints();

        cout << "\n========== VERIFY COMPLAINT ==========\n";

        int id = getComplaintId();

        if (id == -1)
        {
            cout << "\nInvalid Complaint ID.\n";
            return;
        }

        Complaint* complaint = findComplaint(id);

        if (complaint == nullptr)
        {
            cout << "\nComplaint not found.\n";
            return;
        }

        if (complaint->getStatus() != "Pending")
        {
            cout << "\nComplaint is not pending.\n";
            cout << "Current Status: "
                 << complaint->getStatus()
                 << endl;

            return;
        }

        complaint->setStatus("Verified");

        saveComplaints();

        cout << "\nComplaint verified successfully.\n";
    }


    // =================================================
    // 3. APPROVE COMPLAINT
    // =================================================

    void approveComplaint()
    {
        loadComplaints();

        cout << "\n========== APPROVE COMPLAINT ==========\n";

        int id = getComplaintId();

        if (id == -1)
        {
            cout << "\nInvalid Complaint ID.\n";
            return;
        }

        Complaint* complaint = findComplaint(id);

        if (complaint == nullptr)
        {
            cout << "\nComplaint not found.\n";
            return;
        }

        if (complaint->getStatus() != "Verified")
        {
            cout << "\nComplaint must be verified first.\n";

            cout << "Current Status: "
                 << complaint->getStatus()
                 << endl;

            return;
        }

        complaint->setStatus("Approved");

        saveComplaints();

        cout << "\nComplaint approved successfully.\n";
    }


    // =================================================
    // 4. REJECT COMPLAINT
    // =================================================

    void rejectComplaint()
    {
        loadComplaints();

        cout << "\n========== REJECT COMPLAINT ==========\n";

        int id = getComplaintId();

        if (id == -1)
        {
            cout << "\nInvalid Complaint ID.\n";
            return;
        }

        Complaint* complaint = findComplaint(id);

        if (complaint == nullptr)
        {
            cout << "\nComplaint not found.\n";
            return;
        }

        if (complaint->getStatus() == "Resolved")
        {
            cout << "\nResolved complaint cannot be rejected.\n";
            return;
        }

        complaint->setStatus("Rejected");

        saveComplaints();

        cout << "\nComplaint rejected successfully.\n";
    }


    // =================================================
    // 5. MARK DUPLICATE
    // =================================================

    void markDuplicateComplaint()
    {
        loadComplaints();

        cout << "\n========== MARK DUPLICATE ==========\n";

        int id = getComplaintId();

        if (id == -1)
        {
            cout << "\nInvalid Complaint ID.\n";
            return;
        }

        Complaint* complaint = findComplaint(id);

        if (complaint == nullptr)
        {
            cout << "\nComplaint not found.\n";
            return;
        }

        complaint->setStatus("Duplicate");

        saveComplaints();

        cout << "\nComplaint marked as duplicate.\n";
    }


    // =================================================
    // 6. FORWARD COMPLAINT
    // =================================================

    void forwardComplaint()
    {
        loadComplaints();

        cout << "\n========== FORWARD COMPLAINT ==========\n";

        int id = getComplaintId();

        if (id == -1)
        {
            cout << "\nInvalid Complaint ID.\n";
            return;
        }

        Complaint* complaint = findComplaint(id);

        if (complaint == nullptr)
        {
            cout << "\nComplaint not found.\n";
            return;
        }

        if (complaint->getStatus() != "Approved")
        {
            cout << "\nOnly approved complaints can be forwarded.\n";

            cout << "Current Status: "
                 << complaint->getStatus()
                 << endl;

            return;
        }

        complaint->setStatus("Forwarded");

        saveComplaints();

        cout << "\nComplaint forwarded to department.\n";
    }


    // =================================================
    // 7. CHANGE FINAL PRIORITY
    // =================================================

    void changeFinalPriority()
    {
        loadComplaints();

        cout << "\n========== CHANGE FINAL PRIORITY ==========\n";

        int id = getComplaintId();

        if (id == -1)
        {
            cout << "\nInvalid Complaint ID.\n";
            return;
        }

        Complaint* complaint = findComplaint(id);

        if (complaint == nullptr)
        {
            cout << "\nComplaint not found.\n";
            return;
        }


        cout << "\nCurrent Priority: "
             << complaint->getPriority()
             << endl;


        cout << "\n1. Low\n";
        cout << "2. Medium\n";
        cout << "3. High\n";
        cout << "4. Critical\n";

        cout << "Enter choice: ";

        int choice;
        cin >> choice;


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
                cout << "\nInvalid choice.\n";
                return;
        }


        saveComplaints();

        cout << "\nPriority changed successfully.\n";

        cout << "New Priority: "
             << complaint->getPriority()
             << endl;
    }


    // =================================================
    // 8. GENERATE REPORT
    // =================================================

    void generateReports()
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


        cout << "\n============================================\n";
        cout << "           HOSTELCARE REPORT\n";
        cout << "============================================\n";

        cout << "\nTotal Complaints : "
             << complaints.size()
             << endl;

        cout << "\n----- STATUS REPORT -----\n";

        cout << "Pending          : " << pending << endl;
        cout << "Verified         : " << verified << endl;
        cout << "Approved         : " << approved << endl;
        cout << "Rejected         : " << rejected << endl;
        cout << "Duplicate        : " << duplicate << endl;
        cout << "Forwarded        : " << forwarded << endl;
        cout << "Resolved         : " << resolved << endl;
    }


    // =================================================
    // WARDEN MENU
    // =================================================

    void menu()
    {
        int choice;

        do
        {
            cout << "\n========== WARDEN PORTAL ==========\n";

            cout << "1. View Pending Complaints\n";
            cout << "2. Verify Complaint\n";
            cout << "3. Approve Complaint\n";
            cout << "4. Reject Complaint\n";
            cout << "5. Mark Duplicate Complaint\n";
            cout << "6. Forward Complaint to Department\n";
            cout << "7. Change Final Priority\n";
            cout << "8. Generate Reports\n";
            cout << "0. Back to Main Menu\n";

            cout << "Enter your choice: ";

            cin >> choice;


            if (cin.fail())
            {
                cin.clear();

                cin.ignore(
                    numeric_limits<streamsize>::max(),
                    '\n'
                );

                cout << "\nInvalid input.\n";

                continue;
            }


            switch (choice)
            {
                case 1:
                    ::viewPendingComplaints();
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

                case 0:
                    cout << "\nReturning to Main Menu...\n";
                    break;

                default:
                    cout << "\nInvalid choice. Try again.\n";
            }

        } while (choice != 0);
    }
};




void verifyComplaint()
{
    Warden warden;
    warden.verifyComplaint();
}

void approveComplaint()
{
    Warden warden;
    warden.approveComplaint();
}

void rejectComplaint()
{
    Warden warden;
    warden.rejectComplaint();
}

void markDuplicateComplaint()
{
    Warden warden;
    warden.markDuplicateComplaint();
}

void forwardComplaint()
{
    Warden warden;
    warden.forwardComplaint();
}

void changeFinalPriority()
{
    Warden warden;
    warden.changeFinalPriority();
}

void generateReports()
{
    Warden warden;
    warden.generateReports();
}
// =====================================================
// GLOBAL WARDEN MENU FUNCTION
// =====================================================
//
// main.cpp calls wardenMenu(), so we create this
// function and internally use a Warden object.
// =====================================================


void wardenMenu()
{
    Warden warden;
    warden.menu();
}

