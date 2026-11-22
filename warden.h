
#ifndef WARDEN_H
#define WARDEN_H

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <limits>

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
    string hostelName;

    string category;
    string description;

    string priority;
    string status;

    string workerId;
    string feedback;

public:

    // ================= default constructor =================

    Complaint()
    {
        complaintId = 0;
        studentId = 0;
        workerId = "-1";
    }


    // ================= Parameterised constructor =================

    Complaint(
        int cid,
        int sid,
        string sname,
        string room,
        string hostel,
        string cat,
        string desc,
        string pri,
        string stat,
        string wid,
        string feed
    )
    {
        complaintId = cid;
        studentId = sid;
        studentName = sname;
        roomNo = room;
        hostelName = hostel;
        category = cat;
        description = desc;
        priority = pri;
        status = stat;
        workerId = wid;
        feedback = feed;
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

    string getHostelName() const
    {
        return hostelName;
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

    string getWorkerId() const
    {
        return workerId;
    }

    string getFeedback() const
    {
        return feedback;
    }


    // ================= SETTERS =================

    void setStatus(string newStatus)
    {
        status = newStatus;
    }

    void setPriority(string newPriority)
    {
        priority = newPriority;
    }

    void setWorkerId(string newWorkerId)
    {
        workerId = newWorkerId;
    }

    void setFeedback(string newFeedback)
    {
        feedback = newFeedback;
    }

    void display() const;
};


// =====================================================
// WARDEN CLASS
// =====================================================

class Warden
{
private:

    vector<Complaint> complaints;

    const string complaintFile = "data/complaints.txt";


    // ================= file operations =================

    void loadComplaints();

    void saveComplaints();


    // ================= find complaint =================

    Complaint* findComplaint(int id);


    // ================= INPUT =================

    int getComplaintId();


public:

    // ================= view pending complaints =================

    void viewPendingComplaints();

    // ================= complaint management =================

    void verifyComplaint();

    void approveComplaint();

    void rejectComplaint();

    void markDuplicateComplaint();

    void forwardComplaint();

    void changeFinalPriority();

    void generateReports();

    void displayAllComplaints();
    // ================= Warden menu =================

    void menu();
};


// =====================================================
// GLOBAL WARDEN MENU FUNCTION
// =====================================================

void wardenMenu();

#endif
