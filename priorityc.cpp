#include "priority.h"

#include <cctype>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

using namespace std;

namespace
{
    const string STUDENT_FILE = "data/students.txt";
    const string COMPLAINT_FILE = "data/complaints.txt";
    const string TEMP_FILE = "data/complaints.tmp";

    struct StudentRecord
    {
        string id;
        string name;
        string roomNo;
        string contact;
        string hostelName;
    };

    struct ComplaintRecord
    {
        string complaintId;
        string studentId;
        string studentName;
        string roomNo;
        string hostelName;
        string category;
        string description;
        string priority;
        string status;
        string workerId;
        string feedback;
    };

    string lowercase(string text)
    {
        for (char& ch : text)
            ch = static_cast<char>(
                tolower(static_cast<unsigned char>(ch))
            );

        return text;
    }

    bool loadStudents(unordered_map<string, StudentRecord>& students)
    {
        ifstream file(STUDENT_FILE);

        if (!file)
        {
            cerr << "Could not open " << STUDENT_FILE << '\n';
            return false;
        }

        string line;

        while (getline(file, line))
        {
            if (line.empty())
                continue;

            stringstream ss(line);
            StudentRecord student;

            if (!getline(ss, student.id, '|') ||
                !getline(ss, student.name, '|') ||
                !getline(ss, student.roomNo, '|') ||
                !getline(ss, student.contact, '|') ||
                !getline(ss, student.hostelName))
            {
                cerr << "Invalid student record. No complaints were changed.\n";
                return false;
            }

            students[student.id] = student;
        }

        return true;
    }

    bool parseComplaint(const string& line, ComplaintRecord& c)
    {
        stringstream ss(line);

        return
            static_cast<bool>(getline(ss, c.complaintId, '|')) &&
            static_cast<bool>(getline(ss, c.studentId, '|')) &&
            static_cast<bool>(getline(ss, c.studentName, '|')) &&
            static_cast<bool>(getline(ss, c.roomNo, '|')) &&
            static_cast<bool>(getline(ss, c.hostelName, '|')) &&
            static_cast<bool>(getline(ss, c.category, '|')) &&
            static_cast<bool>(getline(ss, c.description, '|')) &&
            static_cast<bool>(getline(ss, c.priority, '|')) &&
            static_cast<bool>(getline(ss, c.status, '|')) &&
            static_cast<bool>(getline(ss, c.workerId, '|')) &&
            static_cast<bool>(getline(ss, c.feedback));
    }

    string calculatePriority(const string& category, const string& description)
    {
        string text = lowercase(category + " " + description);

        // Immediate safety issues
        if (text.find("fire") != string::npos ||
            text.find("smoke") != string::npos ||
            text.find("gas leak") != string::npos ||
            text.find("electric shock") != string::npos ||
            text.find("sparks") != string::npos)
        {
            return "P1";
        }

        // Urgent repairs or security issues
        if (text.find("flood") != string::npos ||
            text.find("pipe burst") != string::npos ||
            text.find("power failure") != string::npos ||
            text.find("no electricity") != string::npos ||
            text.find("lock") != string::npos ||
            text.find("security") != string::npos ||
            text.find("plumbing") != string::npos ||
            text.find("water leak") != string::npos)
        {
            return "P2";
        }

        // Routine repairs
        if (text.find("fan") != string::npos ||
            text.find("light") != string::npos ||
            text.find("bulb") != string::npos ||
            text.find("furniture") != string::npos ||
            text.find("chair") != string::npos ||
            text.find("table") != string::npos ||
            text.find("bed") != string::npos)
        {
            return "P3";
        }

        // Other or routine maintenance
        return "P4";
    }

    bool saveComplaints(const vector<ComplaintRecord>& complaints)
    {
        ofstream file(TEMP_FILE);

        if (!file)
        {
            cerr << "Could not create temporary complaint file.\n";
            return false;
        }

        for (const ComplaintRecord& c : complaints)
        {
            file << c.complaintId << '|'
                 << c.studentId << '|'
                 << c.studentName << '|'
                 << c.roomNo << '|'
                 << c.hostelName << '|'
                 << c.category << '|'
                 << c.description << '|'
                 << c.priority << '|'
                 << c.status << '|'
                 << c.workerId << '|'
                 << c.feedback << '\n';
        }

        file.close();

        if (!file)
        {
            cerr << "Failed while writing the temporary complaint file.\n";
            return false;
        }

        // Replace the original file after the complete output is ready.
        if (remove(COMPLAINT_FILE.c_str()) != 0)
        {
            cerr << "Could not replace the original complaint file. "
                 << "Updated records are in " << TEMP_FILE << '\n';
            return false;
        }

        if (rename(TEMP_FILE.c_str(), COMPLAINT_FILE.c_str()) != 0)
        {
            cerr << "Could not rename the updated file. "
                 << "Updated records are in " << TEMP_FILE << '\n';
            return false;
        }

        return true;
    }
}

void updatePrioritiesFromFiles()
{
    unordered_map<string, StudentRecord> students;

    if (!loadStudents(students))
        return;

    ifstream file(COMPLAINT_FILE);

    if (!file)
    {
        cerr << "Could not open " << COMPLAINT_FILE << '\n';
        return;
    }

    vector<ComplaintRecord> complaints;
    string line;

    while (getline(file, line))
    {
        if (line.empty())
            continue;

        ComplaintRecord complaint;

        if (!parseComplaint(line, complaint))
        {
            cerr << "Invalid complaint record. No complaints were changed.\n";
            return;
        }

        // Refresh student details from students.txt when the ID exists.
        auto student = students.find(complaint.studentId);
        if (student != students.end())
        {
            complaint.studentName = student->second.name;
            complaint.roomNo = student->second.roomNo;
            complaint.hostelName = student->second.hostelName;
        }

        complaint.priority =
            calculatePriority(complaint.category, complaint.description);

        cout << "Complaint " << complaint.complaintId
             << " assigned " << complaint.priority << '\n';

        complaints.push_back(complaint);
    }

    file.close();

    if (saveComplaints(complaints))
        cout << "Complaint priorities updated successfully.\n";
}