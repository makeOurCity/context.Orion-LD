/*
*
* Copyright 2020 FIWARE Foundation e.V.
*
* This file is part of Orion-LD Context Broker.
*
* Orion-LD Context Broker is free software: you can redistribute it and/or
* modify it under the terms of the GNU Affero General Public License as
* published by the Free Software Foundation, either version 3 of the
* License, or (at your option) any later version.
*
* Orion-LD Context Broker is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero
* General Public License for more details.
*
* You should have received a copy of the GNU Affero General Public License
* along with Orion-LD Context Broker. If not, see http://www.gnu.org/licenses/.
*
* For those usages not covered by this license please contact with
* orionld at fiware dot org
*
* Author: Ken Zangelin
*/
#include <postgresql/libpq-fe.h>                               // PGconn

#include "logMsg/logMsg.h"                                     // LM_*
#include "logMsg/traceLevels.h"                                // Lmt*

#include "orionld/troe/pgTransactionBegin.h"                   // pgTransactionBegin
#include "orionld/troe/pgTransactionRollback.h"                // pgTransactionRollback
#include "orionld/troe/pgTransactionCommit.h"                  // pgTransactionCommit
#include "orionld/troe/pgDatabaseTableCreateAll.h"             // Own interface



// -----------------------------------------------------------------------------
//
// pgDatabaseTableCreateAll -
//
// FIXME: The types ValueType+OperationMode needs some investigation
//        It seems like they should be created ONCE for all DBs.
//        If this is true, the creation of the types must be part of the postgres installation.
//        OR: pgInit() creates the types being prepared for failures of type "already exists"
//
bool pgDatabaseTableCreateAll(PGconn* connectionP)
{
  const char* valueTypeSql = "CREATE TYPE ValueType AS ENUM("
    "'String',"
    "'Number',"
    "'Boolean',"
    "'Relationship',"
    "'Compound',"
    "'DateTime',"
    "'Geo',"
    "'LanguageMap')";

  const char* opModeSql = "CREATE TYPE OperationMode AS ENUM("
    "'Create',"
    "'Append',"
    "'Update',"
    "'Replace',"
    "'Delete')";

  const char* entitiesSql = "CREATE TABLE IF NOT EXISTS entities ("
    "instanceId   TEXT PRIMARY KEY,"
    "id           TEXT NOT NULL,"
    "opMode       OperationMode,"
    "type         TEXT NOT NULL,"
    "createdAt    TIMESTAMP NOT NULL,"
    "modifiedAt   TIMESTAMP NOT NULL,"
    "deletedAt    TIMESTAMP)";

  const char* attributesSql = "CREATE TABLE IF NOT EXISTS attributes ("
    "instanceId      TEXT PRIMARY KEY,"
    "id              TEXT NOT NULL,"
    "opMode          OperationMode,"
    "entityRef       TEXT,"
    "entityId        TEXT NOT NULL,"
    "createdAt       TIMESTAMP NOT NULL,"
    "modifiedAt      TIMESTAMP NOT NULL,"
    "deletedAt       TIMESTAMP,"
    "observedAt      TIMESTAMP,"
    "valueType       ValueType,"
    "subProperty     BOOL,"
    "unitCode        TEXT,"
    "datasetId       TEXT,"
    "text            TEXT,"
    "boolean         BOOL,"
    "number          FLOAT8,"
    "datetime        TIMESTAMP)";
  // "geo             GEOMETRY"

  const char* subAttributesSql = "CREATE TABLE IF NOT EXISTS subAttributes ("
    "instanceId      TEXT PRIMARY KEY,"
    "id              TEXT NOT NULL,"
    "entityRef       TEXT,"
    "entityId        TEXT NOT NULL,"
    "attributeRef    TEXT NOT NULL REFERENCES attributes(instanceId),"
    "attributeId     TEXT NOT NULL,"
    "createdAt       TIMESTAMP NOT NULL,"
    "modifiedAt      TIMESTAMP NOT NULL,"
    "deletedAt       TIMESTAMP,"
    "observedAt      TIMESTAMP,"
    "valueType       ValueType,"
    "unitCode        TEXT,"
    "text            TEXT,"
    "boolean         BOOL,"
    "number          FLOAT8,"
    "datetime        TIMESTAMP)";
  // "geo             GEOMETRY"

  const char* sqlV[] = { valueTypeSql, opModeSql, entitiesSql, attributesSql, subAttributesSql };

  if (pgTransactionBegin(connectionP) == false)
    LM_RE(false, ("pgTransactionBegin failed"));

  for (unsigned int ix = 0; ix < sizeof(sqlV) / sizeof(sqlV[0]); ix++)
  {
    LM_TMP(("SQL[%p]: %s", connectionP, sqlV[ix]));
    PGresult*  res = PQexec(connectionP, sqlV[ix]);
    if (res == NULL)
    {
      pgTransactionRollback(connectionP);
      LM_RE(false, ("Database Error (PQexec(%s): %s)", sqlV[ix], PQresStatus(PQresultStatus(res))));
    }
    PQclear(res);
  }

  pgTransactionCommit(connectionP);
  return true;
}