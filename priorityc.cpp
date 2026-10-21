#include "priority.h"

#include <cctype>
#include <fstream>
#include <initializer_list>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace
{
std::string lowercase(std::string text)
{
    for (char& ch : text)
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    return text;
}

bool containsAny(const std::string& text,
                 std::initializer_list<const char*> keywords)
{
    for (const char* keyword : keywords)
        if (text.find(keyword) != std::string::npos)
            return true;
    return false;
}

struct StudentRecord
{
    std::string id;
    std::string name;
    std::string room;
    std::string contact;
    std::string hostel;
};

struct ComplaintRecord
{
    std::string id;
    std::string studentId;
    std::string studentName;
    std::string room;
    std::string hostel;
    std::string category;
    std::string description;
    std::string priority;
    std::string status;
    std::string workerId;
    std::string feedback;
};
}

std::string normalizeComplaintCategory(const std::string& category,
                                       const std::string& description)
{
    const std::string text = lowercase(category + " " + description);

    if (containsAny(text, {"chair", "table", "bed", "desk", "cupboard",
                           "shelf", "furniture", "carpentry", "wood"}))
        return "Furniture / Carpentry";

    if (containsAny(text, {"fire", "smoke", "gas leak", "electric shock",
                           "sparks", "wire", "wiring", "fan", "light",
                           "bulb", "switch", "socket", "electricity",
                           "electrical", "power"}))
        return "Electrical";

    if (containsAny(text, {"tap", "pipe", "leak", "toilet", "drain",
                           "plumbing", "water", "flood"}))
        return "Plumbing";

    if (containsAny(text, {"lock", "door", "gate", "security"}))
        return "Security / Locks";

    if (containsAny(text, {"clean", "dirty", "garbage", "dustbin",
                           "sanitation", "hygiene"}))
        return "Cleaning / Sanitation";

    if (containsAny(text, {"paint", "wall", "ceiling", "floor", "crack",
                           "masonry", "civil"}))
        return "Building / Civil";

    return "General Maintenance";
}

std::string assignComplaintPriority(const std::string& category,
                                    const std::string& description)
{
    const std::string text = lowercase(category + " " + description);

    if (containsAny(text, {"fire", "smoke", "gas leak", "gas leakage",
                           "electric shock", "sparks", "exposed wire"}))
        return "Critical";

    if (containsAny(text, {"flood", "pipe burst", "major water leak",
                           "power failure", "no electricity", "security gate",
                           "gate broken", "lock broken"}))
        return "High";

    if (containsAny(text, {"fan", "light", "bulb", "tap", "pipe", "leak",
                           "toilet", "drain", "lock", "door", "chair",
                           "table", "bed", "desk", "cupboard", "shelf",
                           "furniture", "carpentry", "paint", "wall",
                           "ceiling", "floor", "broken", "repair", "wire",
                           "wiring", "switch", "socket", "electrical"}))
        return "Medium";

    return "Low";
}

void updateComplaintPrioritiesFromFiles()
{
    const char* studentPath = "data/students.txt";
    const char* complaintPath = "data/complaints.txt";

    std::unordered_map<std::string, StudentRecord> students;
    {
        std::ifstream studentFile(studentPath);
        if (!studentFile)
        {
            std::cerr << "Unable to open " << studentPath << ".\n";
            return;
        }

        std::string line;
        while (std::getline(studentFile, line))
        {
            if (line.empty()) continue;
            std::stringstream row(line);
            StudentRecord student;
            if (!std::getline(row, student.id, '|') ||
                !std::getline(row, student.name, '|') ||
                !std::getline(row, student.room, '|') ||
                !std::getline(row, student.contact, '|') ||
                !std::getline(row, student.hostel))
            {
                std::cerr << "Invalid student record; priorities were not updated.\n";
                return;
            }
            students[student.id] = student;
        }
    }

    std::vector<ComplaintRecord> complaints;
    {
        std::ifstream complaintFile(complaintPath);
        if (!complaintFile)
        {
            std::cerr << "Unable to open " << complaintPath << ".\n";
            return;
        }

        std::string line;
        while (std::getline(complaintFile, line))
        {
            if (line.empty()) continue;
            std::stringstream row(line);
            ComplaintRecord c;
            if (!std::getline(row, c.id, '|') ||
                !std::getline(row, c.studentId, '|') ||
                !std::getline(row, c.studentName, '|') ||
                !std::getline(row, c.room, '|') ||
                !std::getline(row, c.hostel, '|') ||
                !std::getline(row, c.category, '|') ||
                !std::getline(row, c.description, '|') ||
                !std::getline(row, c.priority, '|') ||
                !std::getline(row, c.status, '|') ||
                !std::getline(row, c.workerId, '|'))
            {
                std::cerr << "Invalid complaint record; priorities were not updated.\n";
                return;
            }
            // Feedback may legitimately be empty, leaving the line ending
            // immediately after the final '|'.
            std::getline(row, c.feedback);

            const auto student = students.find(c.studentId);
            if (student != students.end())
            {
                c.studentName = student->second.name;
                c.room = student->second.room;
                c.hostel = student->second.hostel;
            }
            c.category = normalizeComplaintCategory(c.category, c.description);
            c.priority = assignComplaintPriority(c.category, c.description);
            complaints.push_back(c);
        }
    }

    // Rewrite the existing file only; no sidecar or temporary data file is made.
    std::ofstream complaintFile(complaintPath, std::ios::trunc);
    if (!complaintFile)
    {
        std::cerr << "Unable to update " << complaintPath << ".\n";
        return;
    }

    for (const ComplaintRecord& c : complaints)
    {
        complaintFile << c.id << '|' << c.studentId << '|' << c.studentName << '|'
                      << c.room << '|' << c.hostel << '|' << c.category << '|'
                      << c.description << '|' << c.priority << '|' << c.status << '|'
                      << c.workerId << '|' << c.feedback << '\n';
    }

    if (!complaintFile)
    {
        std::cerr << "Error while updating complaint data.\n";
        return;
    }
    std::cout << "Automatically assigned priorities to " << complaints.size()
              << " complaint(s).\n";
}

#ifdef PRIORITY_STANDALONE
int main()
{
    updateComplaintPrioritiesFromFiles();
    return 0;
}
#endif
