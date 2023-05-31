
/******************************************************************************
 *    Copyright (C) 2020 NCR Corp. Confidential (Restricted)
 *    
 *    Description:  
 *    History:      2020-12-01 DMP Initial Creation
 *    
 ******************************************************************************/

#pragma once

#include <string>

#ifndef MQTTINTERFACE_API
# define MQTTINTERFACE_API __declspec(dllimport)
#endif 

std::string
EvidTime(const std::string& evid);

std::string
NewEvid();

