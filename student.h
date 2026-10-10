#ifndef STUDENT_H
#define STUDENT_H

#include <iostream>
#include <string>
#include <limits>

using namespace std;

class StudentModule {
private:
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

    const string STUDENT_FILE = "data/students.txt";
    const string COMPLAINT_FILE = "data/complaints.txt";

    void registerStudent();
    void registerComplaint();
    void viewMyComplaints();
    void trackComplaintStatus();
    void viewComplaintHistory();
    void giveFeedback();

    bool findStudent(int id, Student &student);
    int generateComplaintId();

public:
    void studentMenu();
};

#endif
