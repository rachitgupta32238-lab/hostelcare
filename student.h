#ifndef STUDENT_H
#define STUDENT_H

#include <iostream>
#include <string>
#include <limits>

using namespace std;

class StudentModule {
private:
    
struct Student
{
    int studentId;
    string name;
    string roomNo;
    string contact;

    // Default constructor
    Student()
        : studentId(0), name(""), roomNo(""), contact("")
    {
    }

    // Parameterized constructor
    Student(int id, const string& n,
            const string& room, const string& phone)
        : studentId(id), name(n),
          roomNo(room), contact(phone)
    {
    }
};



struct Complaint
{
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

    // Default constructor
    Complaint()
        : complaintId(0), studentId(0),
          studentName(""), roomNo(""),
          category(""), description(""),
          priority("Medium"), status("Pending"),
          workerId(-1), feedback("")
    {
    }

    // Parameterized constructor
    Complaint(int cid, int sid,
              const string& sname, const string& room,
              const string& cat, const string& desc,
              const string& pr, const string& st,
              int wid, const string& fb)
        : complaintId(cid), studentId(sid),
          studentName(sname), roomNo(room),
          category(cat), description(desc),
          priority(pr), status(st),
          workerId(wid), feedback(fb)
    {
    }
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
