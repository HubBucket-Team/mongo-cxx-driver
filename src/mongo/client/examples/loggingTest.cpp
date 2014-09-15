/*    Copyright 2014 MongoDB Inc.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#if defined(_WIN32)
#include <winsock2.h>
#include <windows.h>
#endif

#include <iostream>
#include "mongo/client/dbclient.h"
#include "mongo/stdx/functional.h"

// NOTE:
// log.h is a private header and is not meant to be included in user
// applications.  We include it here only as a test, to simulate the
// driver's internal logging functionality.
#include "mongo/util/log.h"

#ifndef verify
#  define verify(x) MONGO_verify(x)
#endif

using namespace std;
using namespace mongo;
using namespace logger;

namespace {

    // for testing, this appender simply counts the number of
    // messages it has received.
    class CountAppender : public MessageLogDomain::EventAppender {
    public:
        explicit CountAppender(AtomicWord<int>* counter) : _count(counter) {}

        virtual Status append(const MessageLogDomain::EventAppender::Event& event) {
            if (event.getMessage().find("[COUNT]") != string::npos) {
                _count->fetchAndAdd(1);
            }
            return Status::OK();
        }

        int getCount() { return _count->load(); }

    private:
        AtomicWord<int>* _count;
    };

    // A simple fetch function to use for our factory
    client::Options::LogAppenderPtr makeCountAppender(AtomicWord<int>* counter) {
        return client::Options::LogAppenderPtr(new CountAppender(counter));
    }

} // namespace

int main() {

    client::Options loggingOpts = client::Options();
    AtomicWord<int> logCounter;

    // add an appender
    client::Options::LogAppenderFactory factory =
        stdx::bind(&makeCountAppender, &logCounter);
    loggingOpts.setLogAppenderFactory(factory);
    verify(loggingOpts.logAppenderFactory());

    // set the logging level twice, see that the later one sticks
    LogSeverity level1 = LogSeverity::Debug(2);
    loggingOpts.setMinLoggedSeverity(level1);
    verify(loggingOpts.minLoggedSeverity() == level1);

    LogSeverity level2 = LogSeverity::Debug(1);
    loggingOpts.setMinLoggedSeverity(level2);
    verify(loggingOpts.minLoggedSeverity() == level2);

    // initialize with our options
    Status status = client::initialize(loggingOpts);
    if (!status.isOK()) {
        cout << "Failed to initialize the driver" << endl;
        return EXIT_FAILURE;
    }

    // Check that logging is working as expected
    MONGO_LOG(3) << "[COUNT] I'm a log message!" << endl;
    MONGO_LOG(2) << "[COUNT] Quick, run!" << endl;
    MONGO_LOG(1) << "[COUNT] Phew, the coast is clear." << endl;
    MONGO_LOG(0) << "[COUNT] Excellent." << endl;
    verify(logCounter.load() == 2);

    return EXIT_SUCCESS;
}
