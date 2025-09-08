#ifndef RECORDING_H
#define RECORDING_H

#include <vector>
#include <string>

class SessionRecorder {
public:
    SessionRecorder();
    ~SessionRecorder();

    bool startRecording(const std::string& sessionId);
    bool stopRecording();
    bool saveRecording(const std::string& filePath);
    std::vector<std::string> getRecordedSessions() const;

private:
    std::vector<std::string> recordedSessions;
    bool isRecording;
    std::string currentSessionId;
};

#endif // RECORDING_H