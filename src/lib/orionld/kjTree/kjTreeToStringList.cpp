/*
*
* Copyright 2019 Telefonica Investigacion y Desarrollo, S.A.U
*
* This file is part of Orion Context Broker.
*
* Orion Context Broker is free software: you can redistribute it and/or
* modify it under the terms of the GNU Affero General Public License as
* published by the Free Software Foundation, either version 3 of the
* License, or (at your option) any later version.
*
* Orion Context Broker is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero
* General Public License for more details.
*
* You should have received a copy of the GNU Affero General Public License
* along with Orion Context Broker. If not, see http://www.gnu.org/licenses/.
*
* For those usages not covered by this license please contact with
* iot_support at tid dot es
*
* Author: Ken Zangelin
*/
#include <string>                                              // std::string
#include <vector>                                              // std::vector

#include "logMsg/logMsg.h"                                     // LM_*
#include "logMsg/traceLevels.h"                                // Lmt*

extern "C"
{
#include "kjson/KjNode.h"                                      // KjNode
}

#include "orionld/common/CHECK.h"                              // CHECKx()
#include "orionld/common/orionldState.h"                       // orionldState
#include "orionld/common/orionldErrorResponse.h"               // orionldErrorResponseCreate
#include "orionld/context/orionldUriExpand.h"                  // orionldUriExpand
#include "orionld/kjTree/kjTreeToStringList.h"                 // Own interface



// -----------------------------------------------------------------------------
//
// kjTreeToStringList -
//
bool kjTreeToStringList(ConnectionInfo* ciP, KjNode* kNodeP, std::vector<std::string>* stringListP)
{
  KjNode* attributeP;

  for (attributeP = kNodeP->value.firstChildP; attributeP != NULL; attributeP = attributeP->next)
  {
    char  expanded[256];
    char* details;

    STRING_CHECK(attributeP, "String-List item");

    if (orionldUriExpand(orionldState.contextP, attributeP->value.s, expanded, sizeof(expanded), &details) == false)
    {
      orionldErrorResponseCreate(OrionldBadRequestData, "Error during URI expansion of entity type", details, OrionldDetailsString);
      delete stringListP;  // ?
      return false;
    }

    stringListP->push_back(expanded);
  }

  return true;
}
