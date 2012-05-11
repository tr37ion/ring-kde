#include "sflphoneService.h"

#include "../../src/lib/Call.h"

SFLPhoneService::SFLPhoneService(SFLPhoneEngine *engine)

{
    m_engine = engine;
    setName("sflphone");
}

ServiceJob *SFLPhoneService::createJob(const QString &operation, QMap<QString, QVariant> &parameters)
{
    if (!m_engine) {
        return 0;
    }

    if (operation == "Call") {
       return new CallJob(this, operation,parameters);
    }
    qDebug() << "\n\n\nIN SERVICE" << parameters["query"];
    m_engine->setData(operation, parameters["query"]);
    return 0;
}