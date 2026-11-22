#ifndef PRIORITY_H
#define PRIORITY_H

#include <string>

// Groups descriptions under a shared category and assigns a warden-compatible
// priority label (Critical, High, Medium, or Low).
std::string normalizeComplaintCategory(const std::string& category,
                                       const std::string& description);
std::string assignComplaintPriority(const std::string& category,
                                    const std::string& description);
void updateComplaintPrioritiesFromFiles();

#endif
