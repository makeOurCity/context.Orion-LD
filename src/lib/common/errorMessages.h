#ifndef SRC_LIB_COMMON_ERRORMESSAGES_H
#define SRC_LIB_COMMON_ERRORMESSAGES_H

/*
*
* Copyright 2016 Telefonica Investigacion y Desarrollo, S.A.U
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
* Author: Orion dev team
*/
#include "common/limits.h"



/* ****************************************************************************
*
* Auxiliar macros for stringification of integers.
* See http://stackoverflow.com/questions/5459868/c-preprocessor-concatenate-int-to-string
*/
#define STR2(x) #x
#define STR(x)  STR2(x)



#define MORE_MATCHING_ENT   "More than one matching entity. Please refine your query"
#define INVAL_CHAR_URI      "invalid character in URI"
#define EMPTY_ENTITY_ID     "entity id length: 0, min length supported: "      STR(MIN_ID_LEN)
#define EMPTY_ATTR_NAME     "attribute name length: 0, min length supported: " STR(MIN_ID_LEN)
#define EMPTY_ENTITY_TYPE   "entity type length: 0, min length supported: "    STR(MIN_ID_LEN)
#define BAD_VERB            "method not allowed"

#endif // SRC_LIB_COMMON_ERRORMESSAGES_H