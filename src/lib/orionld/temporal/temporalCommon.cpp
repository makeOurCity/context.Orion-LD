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
* Author: Chandra Challagonda & Ken Zangelin
*/
#include <string.h>                                              // strlen

extern "C"
{
#include "kbase/kTime.h"                                         // kTimeGet
#include "kjson/kjBufferCreate.h"                                // kjBufferCreate
#include "kjson/kjFree.h"                                        // kjFree
#include "kjson/kjLookup.h"                                      // kjLookup
#include "kalloc/kaBufferInit.h"                                 // kaBufferInit
#include "kjson/kjRender.h"                                      // kjRender
#include "kjson/kjBuilder.h"                                     // kjChildRemove .....
}

#include "logMsg/logMsg.h"                                       // LM_*
#include "logMsg/traceLevels.h"                                  // Lmt*
#include "orionld/common/uuidGenerate.h"                                 // for uuidGenerate

#include "rest/ConnectionInfo.h"                               // ConnectionInfo
#include "rest/HttpStatusCode.h"                               // SccNotImplemented
#include "orionld/common/orionldState.h"                       // orionldState
#include "orionld/common/orionldErrorResponse.h"               // orionldErrorResponseCreate
#include "orionld/rest/OrionLdRestService.h"                   // OrionLdRestService
#include "orionld/types/OrionldGeoIndex.h"                       // OrionldGeoIndex
#include "orionld/db/dbConfiguration.h"                          // DB_DRIVER_MONGOC
#include "orionld/context/orionldCoreContext.h"                  // orionldCoreContext
#include "orionld/common/QNode.h"                                // QNode
#include "orionld/common/orionldTenantCreate.h"                // Own interface
#include "orionld/context/orionldContextItemExpand.h"            //  orionldContextItemExpand

#include "orionld/temporal/temporalCommon.h"                     // Temporal common


PGconn* oldPgDbConnection = NULL;
PGconn* oldPgDbTenantConnection = NULL;
PGresult* oldPgTenandDbResult = NULL;
//OrionldTemporalDbEntityTable* dbEntityTableLocal;
//OrionldTemporalDbAttributeTable* dbAttributeTableLocal;

static const char* dbValueEnumString(OrionldTemporalAttributeValueTypeEnum enumValueType)
{
  switch (enumValueType)
  {
    case EnumValueString:
      LM_TMP (("It is value_string"));
      return "value_string";
      break;

    case EnumValueNumber:
      LM_TMP (("It is value_number"));
      return "value_number";
      break;

    case EnumValueBool:
      LM_TMP (("It is value_boolean"));
      return "value_boolean";
      break;

    case EnumValueArray:
      LM_TMP (("It is value_array"));
      return "value_array";
      break;

    case EnumValueRelation:
      LM_TMP (("It is value_relation"));
      return "value_relation";
      break;

    case EnumValueObject:
      LM_TMP (("It is value_object"));
      return "value_object";
      break;

    case EnumValueDateTime:
      LM_TMP (("It is value_datetime"));
      return "value_datetime";
      break;

    default:
      LM_W(("Error - Invalid attribute Value type %d", enumValueType));
      return NULL;
    }
}



void entityExtract (OrionldTemporalDbAllTables* allTab, KjNode* entityP, bool arrayFlag, int entityIndex)
{
  if(arrayFlag)
  {
    KjNode* idP = kjLookup(entityP, "id");
    KjNode* typeP = kjLookup(entityP, "type");
    //KjNode* createdAtP = kjLookup(entityP, "createdAt");
    //KjNode* modifiedAtP = kjLookup(entityP, "modifiedAt");

    allTab->entityTableArray[entityIndex].entityId = idP->value.s;
    allTab->entityTableArray[entityIndex].entityType = (typeP != NULL)? typeP->value.s : NULL;
    kjChildRemove (entityP, idP);
    if (typeP != NULL)
      kjChildRemove(entityP, typeP);
  }
  else
  {
    allTab->entityTableArray[entityIndex].entityId = orionldState.payloadIdNode->value.s;
    allTab->entityTableArray[entityIndex].entityType = (orionldState.payloadTypeNode != NULL)? orionldState.payloadTypeNode->value.s : NULL;
    allTab->entityTableArray[entityIndex].createdAt = orionldState.timestamp.tv_sec + ((double) orionldState.timestamp.tv_nsec) / 1000000000;
    allTab->entityTableArray[entityIndex].modifiedAt = orionldState.timestamp.tv_sec + ((double) orionldState.timestamp.tv_nsec) / 1000000000;
  }

  allTab->entityTableArray[entityIndex].createdAt = orionldState.timestamp.tv_sec + ((double) orionldState.timestamp.tv_nsec) / 1000000000;
  allTab->entityTableArray[entityIndex].modifiedAt = orionldState.timestamp.tv_sec + ((double) orionldState.timestamp.tv_nsec) / 1000000000;

  int attributesCount = 0;
  int subAttrCount = 0;

  for (KjNode* attrP = orionldState.requestTree->value.firstChildP; attrP != NULL; attrP = attrP->next)
  {
    attributesCount++;

    for (KjNode* subAttrP = attrP->value.firstChildP; subAttrP != NULL; subAttrP = subAttrP->next)
    {
      LM_TMP(("CCSR: Found SubAttribute\n"));
      subAttrCount++;
    }
  }

  LM_K(("CCSR: Number of Attributes & number of SubAttributes %i %i", attributesCount, subAttrCount));

  allTab->attributeTableArrayItems = attributesCount;
  allTab->subAttributeTableArrayItems = subAttrCount;

  int attribArrayTotalSize = attributesCount * sizeof(OrionldTemporalDbAttributeTable);
  allTab->attributeTableArray = (OrionldTemporalDbAttributeTable*) kaAlloc(&orionldState.kalloc, attribArrayTotalSize);
  bzero(allTab->attributeTableArray, attribArrayTotalSize);
  LM_TMP(("CCSR: AttrArrayTotalSize:%d", attribArrayTotalSize));
  LM_TMP(("CCSR: attributesCount:%d", attributesCount));

  int subAttribArrayTotalSize = subAttrCount * sizeof(OrionldTemporalDbSubAttributeTable);
  allTab->subAttributeTableArray = (OrionldTemporalDbSubAttributeTable*) kaAlloc(&orionldState.kalloc, subAttribArrayTotalSize);
  bzero(allTab->subAttributeTableArray, subAttribArrayTotalSize);

  int attrIndex=0;
  int subAttrIndex = 0;
  for (KjNode* attrP = orionldState.requestTree->value.firstChildP; attrP != NULL; attrP = attrP->next)
  {
    LM_K(("CCSR: Before callig attrExtract"));
    allTab->attributeTableArray[attrIndex].entityId = allTab->entityTableArray[entityIndex].entityId;
    attrExtract (attrP, &allTab->attributeTableArray[attrIndex], allTab->subAttributeTableArray , attrIndex, &subAttrIndex);
    attrIndex++;
  }
}


// -----------------------------------------------------------------------------
//
// temporalOrionldCommonExtractTree - initialize the thread-local variables of temporalOrionldCommonState
// INSERT INTO entity_table(entity_id,entity_type,geo_property,created_at,modified_at, observed_at)
//      VALUES ("%s,%s,%s,%s");
//
OrionldTemporalDbAllTables*  temporalEntityExtract()
{
    OrionldTemporalDbAllTables*          dbAllTablesLocal; // Chandra - TBI
    //  OrionldTemporalDbEntityTable*        dbEntityTableLocal;
    //  OrionldTemporalDbAttributeTable*     dbAttributeTableLocal;
    //  OrionldTemporalDbSubAttributeTable*  dbSubAttributeTableLocal;


    int dbAllTablesSize = sizeof(OrionldTemporalDbAllTables);
    dbAllTablesLocal = (OrionldTemporalDbAllTables*) kaAlloc(&orionldState.kalloc, dbAllTablesSize);
    bzero(dbAllTablesLocal, dbAllTablesSize);

    dbAllTablesLocal->entityTableArrayItems = 0;

    //dbAllTablesLocal->attributeTableArray = dbAttributeTableLocal;
    //dbAllTablesLocal->subAttributeTableArray = dbSubAttributeTableLocal;

    //  orionldState.requestTree->type == KjArray;
    if(orionldState.requestTree->type == KjArray)
    {
      int entityCount = 0;
      for (KjNode* entityP = orionldState.requestTree->value.firstChildP; entityP != NULL; entityP = entityP->next)
      {
         entityCount++;
      }

      LM_K(("Number of Entities after count %i", entityCount));


      dbAllTablesLocal->entityTableArray = (OrionldTemporalDbEntityTable*) kaAlloc(&orionldState.kalloc, (entityCount * sizeof(OrionldTemporalDbEntityTable)) );
      bzero(dbAllTablesLocal->entityTableArray, (entityCount * sizeof(OrionldTemporalDbEntityTable)));

      //dbAllTablesLocal->entityTableArray = dbEntityTableLocal;

      int entityIndex=0;
      for(KjNode* entityP = orionldState.requestTree->value.firstChildP; entityP != NULL; entityP = entityP->next)
      {
        entityExtract (dbAllTablesLocal, entityP, true, entityIndex++);
        dbAllTablesLocal->entityTableArrayItems++;
      }
    }
    else
    {
      dbAllTablesLocal->entityTableArray = (OrionldTemporalDbEntityTable*) kaAlloc(&orionldState.kalloc, sizeof(OrionldTemporalDbEntityTable));
      bzero(dbAllTablesLocal->entityTableArray, sizeof(OrionldTemporalDbEntityTable));

      //dbAllTablesLocal->entityTableArray = dbEntityTableLocal;

      entityExtract (dbAllTablesLocal, orionldState.requestTree, false, 0);
      dbAllTablesLocal->entityTableArrayItems++;
    }

    LM_K(("Number of Entities %i", dbAllTablesLocal->entityTableArrayItems));

#if 0
}


    else
    {
      OrionldTemporalDbEntityTable*        dbEntityTableLocal;
      OrionldTemporalDbAttributeTable*     dbAttributeTableLocal;
      OrionldTemporalDbSubAttributeTable** dbSubAttributeTableLocal;

      int entityArrayTotalSize = sizeof(OrionldTemporalDbEntityTable);
      dbEntityTableLocal = (OrionldTemporalDbEntityTable*) kaAlloc(&orionldState.kalloc, entityArrayTotalSize);
      bzero(dbEntityTableLocal, entityArrayTotalSize);

      dbEntityTableLocal[0].entityId = orionldState.payloadIdNode->value.s;
      dbEntityTableLocal[0].entityType = orionldState.payloadTypeNode->value.s;

/*
#if 0
#else
#endif
*/

      int attributesNumbers = 0;
      for (KjNode* attrP = orionldState.requestTree->value.firstChildP; attrP != NULL; attrP = attrP->next)
      {
         attributesNumbers++;
      }

      int attribArrayTotalSize = attributesNumbers * sizeof(OrionldTemporalDbAttributeTable);
      dbAttributeTableLocal = (OrionldTemporalDbAttributeTable*) kaAlloc(&orionldState.kalloc, attribArrayTotalSize);
      bzero(dbAttributeTableLocal, attribArrayTotalSize);

      dbSubAttributeTableLocal = (OrionldTemporalDbSubAttributeTable**) kaAlloc(&orionldState.kalloc, (attributesNumbers * sizeof(OrionldTemporalDbSubAttributeTable*)) );
      bzero(dbSubAttributeTableLocal, (attributesNumbers * sizeof(OrionldTemporalDbSubAttributeTable*)));

      int attrIndex=0;
      for (KjNode* attrP = orionldState.requestTree->value.firstChildP; attrP != NULL; attrP = attrP->next)
      {
         dbAttributeTableLocal[attrIndex].entityId = dbEntityTableLocal[0].entityId;
         attrExtract (attrP, &dbAttributeTableLocal[attrIndex], dbSubAttributeTableLocal, attrIndex);
         attrIndex++;
      }

      dbAllTablesLocal->entityTableArray = dbEntityTableLocal;
      dbAllTablesLocal->attributeTableArray = dbAttributeTableLocal;
      dbAllTablesLocal->subAttributeTableArray = *dbSubAttributeTableLocal;
    }

#endif
  return dbAllTablesLocal;
}


void attrExtract(KjNode* attrP, OrionldTemporalDbAttributeTable* dbAttributeTableLocal,
  OrionldTemporalDbSubAttributeTable* dbSubAttributeTableLocal, int attrIndex, int* subAttrIndexP)
{
    int subAttrIx = *subAttrIndexP;

    dbAttributeTableLocal->attributeName = attrP->name;

    if (attrP->type != KjObject)
    {
        LM_W(("Temporal - Bad Input - Key values not supported"));
        return;
    }

    KjNode* observedAtP = kjLookup(attrP, "observedAt");
    if (observedAtP != NULL)
    {
        //if (observedAtP->type == KjString)
        //    dbAttributeTableLocal->observedAt = parse8601Time(observedAtP->value.s);
        //else
        dbAttributeTableLocal->observedAt = 49;
        kjChildRemove (attrP,observedAtP);
        LM_TMP(("CCSR - Temporal - Found observedAt %s",observedAtP->value.s));
    }

    KjNode* nodeP  = kjLookup(attrP, "unitCode");
    if(nodeP != NULL)
    {
        kjChildRemove (attrP,nodeP);
        // Chandra-TBI
    }

    nodeP  = kjLookup(attrP, "location");
    if(nodeP != NULL)
    {
        kjChildRemove (attrP,nodeP);
        LM_TMP(("CCSR - Found Location "));
        // Chandra-TBI
    }

    nodeP  = kjLookup(attrP, "operationSpace");
    if(nodeP != NULL)
    {
        kjChildRemove (attrP,nodeP);
        // Chandra-TBI
    }

    nodeP  = kjLookup(attrP, "observationSpace");
    if(nodeP != NULL)
    {
        kjChildRemove (attrP,nodeP);
        // Chandra-TBI
    }

    nodeP  = kjLookup(attrP, "datasetId");
    if(nodeP != NULL)
    {
        kjChildRemove (attrP,nodeP);
        // Chandra-TBI
    }

    //nodeP  = kjLookup(attrP, "instanceid");
    //if(nodeP != NULL)
    //{
    //    kjChildRemove (attrP,nodeP);
    //    // Chandra-TBI
    //}

    KjNode* attrTypeP  = kjLookup(attrP, "type");
    kjChildRemove (attrP,attrTypeP);
    dbAttributeTableLocal->attributeType = attrTypeP->value.s;

    if (strcmp (dbAttributeTableLocal->attributeType,"Relationship") == 0)
    // if (dbAttributeTableLocal[oldTemporalTreeNodeLevel].attributeType == "Relationship")
    {
        KjNode* attributeObject  = kjLookup(attrP, "object");
        kjChildRemove (attrP,attributeObject);

        // dbEntityTableLocal.attributeValueType  = kjLookup(attrP, "object");
        dbAttributeTableLocal->attributeValueType  = EnumValueRelation;
        dbAttributeTableLocal->valueString = attributeObject->value.s;
        LM_TMP(("CCSR:  Relationship : '%s'", dbAttributeTableLocal->valueString));

    }
    else if (strcmp (dbAttributeTableLocal->attributeType,"GeoProperty") == 0)
    // if (dbAttributeTableLocal[oldTemporalTreeNodeLevel].attributeType == "GeoProperty")
    {
        KjNode* valueP  = kjLookup(attrP, "value");  //Chandra-TBD
        kjChildRemove (attrP,valueP);

      //dbAttributeTableLocal->geoPropertyType = valueP->type.s;
        // Chandra-TBI
        LM_TMP(("CCSR:  Found GeoProperty : "));

    }
    else if (strcmp (dbAttributeTableLocal->attributeType,"Property") == 0)
    // if (dbAttributeTableLocal[oldTemporalTreeNodeLevel].attributeType == "Property")
    {
        KjNode* valueP  = kjLookup(attrP, "value");  //Chandra-TBD
        kjChildRemove (attrP,valueP);

        if (valueP->type == KjFloat)
        {
              dbAttributeTableLocal->attributeValueType  = EnumValueNumber;
              dbAttributeTableLocal->valueNumber = valueP->value.f;
              LM_TMP(("CCSR:  attribute value number  : %f", dbAttributeTableLocal->valueNumber));
        }
        else if (valueP->type == KjInt)
        {
              dbAttributeTableLocal->attributeValueType  = EnumValueNumber;
              dbAttributeTableLocal->valueNumber = valueP->value.i;
              LM_TMP(("CCSR:  attribute value number  : %i", dbAttributeTableLocal->valueNumber));
        }
        else if (valueP->type == KjArray)
        {
              dbAttributeTableLocal->attributeValueType  = EnumValueArray;
              dbAttributeTableLocal->valueString = kaAlloc(&orionldState.kalloc, 1024); //Chandra-TBD Not smart
              kjRender(orionldState.kjsonP, valueP->value.firstChildP, dbAttributeTableLocal->valueString, 1024);
        }
        else if (valueP->type == KjObject)
        {
              dbAttributeTableLocal->attributeValueType  = EnumValueObject;
              dbAttributeTableLocal->valueString = kaAlloc(&orionldState.kalloc, 1024); //Chandra-TBD Not smart
              kjRender(orionldState.kjsonP, valueP->value.firstChildP, dbAttributeTableLocal->valueString, 1024);
        }
        else if (valueP->type == KjBoolean)
        {
              dbAttributeTableLocal->attributeValueType  = EnumValueBool;
              dbAttributeTableLocal->valueNumber = valueP->value.b;

        }
        else if (valueP->type == KjString)
        {
              dbAttributeTableLocal->attributeValueType  = EnumValueString;
              dbAttributeTableLocal->valueString = valueP->value.s;
              LM_TMP(("CCSR:  attribute value string  : %s", dbAttributeTableLocal->valueString));
        }

        LM_TMP(("CCSR:  Attribute Value type : %d", dbAttributeTableLocal->attributeValueType));
    }

    //  adding instance id to map to sub attributes
    dbAttributeTableLocal->instanceId = kaAlloc(&orionldState.kalloc, 64);
    uuidGenerate(dbAttributeTableLocal->instanceId);

    // Now we look the special sub attributes - unitCode, observacationspace, dataSetId, instanceid, location & operationSpace


    if (attrP->value.firstChildP != NULL)
    {
      LM_TMP(("CCSR:  Tring to extract subattribute "));

        dbAttributeTableLocal->subProperty = true;

        for (KjNode* subAttrP = attrP->value.firstChildP; subAttrP != NULL; subAttrP = subAttrP->next)
        {
            dbSubAttributeTableLocal[subAttrIx].attributeName = dbAttributeTableLocal->attributeName;
            dbSubAttributeTableLocal[subAttrIx].attrInstanceId = dbAttributeTableLocal->instanceId;
            attrSubAttrExtract (subAttrP, &dbSubAttributeTableLocal[subAttrIx]);
            subAttrIx++;
        }
        *subAttrIndexP = subAttrIx;
    }

    dbAttributeTableLocal->createdAt = orionldState.timestamp.tv_sec + ((double) orionldState.timestamp.tv_nsec) / 1000000000;
    dbAttributeTableLocal->modifiedAt = orionldState.timestamp.tv_sec + ((double) orionldState.timestamp.tv_nsec) / 1000000000;
     //oldTemporalTreeNodeLevel++;
     //}
}



void  attrSubAttrExtract(KjNode* subAttrP, OrionldTemporalDbSubAttributeTable* dbSubAttributeTableLocal)
{
    //for (KjNode* attrP = orionldState.requestTree->value.firstChildP; attrP != NULL; attrP = attrP->next)
    //{
     dbSubAttributeTableLocal->subAttributeName = subAttrP->name;
     if (subAttrP->type != KjObject)
     {
         LM_W(("Temporal - Bad Input - Key values not supported"));
         return;
     }

     KjNode* attrTypeP  = kjLookup(subAttrP, "type");
     kjChildRemove (subAttrP,attrTypeP);
     dbSubAttributeTableLocal->subAttributeType = attrTypeP->value.s;

     if (strcmp (dbSubAttributeTableLocal->subAttributeType,"Relationship") == 0)
     // if (dbAttributeTableLocal[oldTemporalTreeNodeLevel].attributeType == "Relationship")
     {
         KjNode* attributeObject  = kjLookup(subAttrP, "object");
         kjChildRemove (subAttrP,attributeObject);

         // dbEntityTableLocal.attributeValueType  = kjLookup(attrP, "object");
         dbSubAttributeTableLocal->subAttributeValueType  = EnumValueRelation;
         dbSubAttributeTableLocal->subAttributeValueString = attributeObject->value.s;
         LM_TMP(("CCSR:  Relationship : '%s'", dbSubAttributeTableLocal->subAttributeValueString));

     }
     else if (strcmp (dbSubAttributeTableLocal->subAttributeType,"GeoProperty") == 0)
     // if (dbAttributeTableLocal[oldTemporalTreeNodeLevel].attributeType == "GeoProperty")
     {
         KjNode* valueP  = kjLookup(subAttrP, "value");  //Chandra-TBD
         kjChildRemove (subAttrP,valueP);
         LM_TMP(("CCSR:  Found GeoProperty : "));         // Chandra-TBI
     }
     else if (strcmp (dbSubAttributeTableLocal->subAttributeType,"Property") == 0)
     // if (dbAttributeTableLocal[oldTemporalTreeNodeLevel].attributeType == "Property")
     {
         KjNode* valueP  = kjLookup(subAttrP, "value");  //Chandra-TBD
         kjChildRemove (subAttrP,valueP);

         if (valueP->type == KjFloat)
         {
               dbSubAttributeTableLocal->subAttributeValueType  = EnumValueNumber;
               dbSubAttributeTableLocal->subAttributeValueNumber = valueP->value.f;
         }
         else if (valueP->type == KjInt)
         {
               dbSubAttributeTableLocal->subAttributeValueType  = EnumValueNumber;
               dbSubAttributeTableLocal->subAttributeValueNumber = valueP->value.i;
         }
         else if (valueP->type == KjArray)
         {
               dbSubAttributeTableLocal->subAttributeValueType  = EnumValueArray;
               dbSubAttributeTableLocal->subAttributeValueString = kaAlloc(&orionldState.kalloc, 1024); //Chandra-TBD Not smart
               kjRender(orionldState.kjsonP, valueP->value.firstChildP, dbSubAttributeTableLocal->subAttributeValueString, 1024);
         }
         else if (valueP->type == KjObject)
         {
               dbSubAttributeTableLocal->subAttributeValueType  = EnumValueObject;
               dbSubAttributeTableLocal->subAttributeValueString = kaAlloc(&orionldState.kalloc, 1024); //Chandra-TBD Not smart
               kjRender(orionldState.kjsonP, valueP->value.firstChildP, dbSubAttributeTableLocal->subAttributeValueString, 1024);
         }
         else if (valueP->type == KjBoolean)
         {
               dbSubAttributeTableLocal->subAttributeValueType  = EnumValueBool;
               dbSubAttributeTableLocal->subAttributeValueNumber = valueP->value.b;

         }
         else if (valueP->type == KjString)
         {
               dbSubAttributeTableLocal->subAttributeValueType  = EnumValueString;
               dbSubAttributeTableLocal->subAttributeValueString = valueP->value.s;
         }
       }

       // Now we look the special sub attributes - unitCode, observacationspace, dataSetId, instanceid, location & operationSpace
       KjNode* nodeP  = kjLookup(subAttrP, "unitCode");
       if(nodeP != NULL)
       {
           kjChildRemove (subAttrP,nodeP);
           // Chandra-TBI
       }

       nodeP  = kjLookup(subAttrP, "observationSpace");
       if(nodeP != NULL)
       {
           kjChildRemove (subAttrP,nodeP);
           // Chandra-TBI
       }

       nodeP  = kjLookup(subAttrP, "datasetId");
       if(nodeP != NULL)
       {
           kjChildRemove (subAttrP,nodeP);
           // Chandra-TBI
       }

       nodeP  = kjLookup(subAttrP, "instanceid");
       if(nodeP != NULL)
       {
           kjChildRemove (subAttrP,nodeP);
           // Chandra-TBI
       }

       nodeP  = kjLookup(subAttrP, "location");
       if(nodeP != NULL)
       {
           kjChildRemove (subAttrP,nodeP);
           // Chandra-TBI
       }

       nodeP  = kjLookup(subAttrP, "operationSpace");
       if(nodeP != NULL)
       {
           kjChildRemove (subAttrP,nodeP);
           // Chandra-TBI
       }

       /*if (attrP->value.firstChildP != NULL)  //Chandra-TBCKZ (Can we safely assume that there are sub attributes to sub attributes?)
       {
           dbAttributeTableLocal[oldTemporalTreeNodeLevel].subProperty = true;
           int subAttrs = 0;
           for (KjNode* subAttrP = attrP->value.firstChildP; subAttrP != NULL; subAttrP = subAttrP->next)
           {
                   subAttrs++;
           }

           int subAttribArrayTotalSize = subAttrs * sizeof(OrionldTemporalDbSubAttributeTable);
           dbSubAttributeTableLocal = (OrionldTemporalDbSubAttributeTable*) kaAlloc(&orionldState.kalloc, subAttribArrayTotalSize);
           bzero(dbSubAttributeTableLocal, subAttribArrayTotalSize);

           int subAttrIx=0;
           for (KjNode* subAttrP = attrP->value.firstChildP; subAttrP != NULL; subAttrP = subAttrP->next)
           {
                   subAttrExtract (subAttrP, &dbSubAttributeTableLocal[subAttrIx++]);
           }

       }*/

       dbSubAttributeTableLocal->createdAt = orionldState.timestamp.tv_sec + ((double) orionldState.timestamp.tv_nsec) / 1000000000;
       dbSubAttributeTableLocal->modifiedAt = orionldState.timestamp.tv_sec + ((double) orionldState.timestamp.tv_nsec) / 1000000000;

       KjNode* observedAtP = kjLookup(subAttrP, "observedAt");
       if (observedAtP != NULL)
       {
          //if (observedAtP->type == KjString)
          //   dbSubAttributeTableLocal->observedAt = parse8601Time(observedAtP->value.s);
          //else
             dbSubAttributeTableLocal->observedAt = observedAtP->value.f;
       }
    // }
}




// ----------------------------------------------------------------------------
//
// TemporalPgDBConnectorOpen(PGconn* conn) - function to close the Postgres database connection gracefully
//
// ----------------------------------------------------------------------------
bool TemporalPgDBConnectorClose()
{
    if(oldPgDbTenantConnection != NULL)
    {
          PQfinish(oldPgDbTenantConnection); // Closes the TenantDB connection
    }


    if(oldPgDbConnection != NULL)
    {
    	PQfinish(oldPgDbConnection); //Closes connection and and also frees memory used by the PGconn* conn variable
    }
    else
    {
    	return false;
    }

    return true;
}

// ----------------------------------------------------------------------------
//
// bool TemporalPgDBConnectorOpen(char *tenantName) - function to open the Postgres database connection
//
// ----------------------------------------------------------------------------
bool TemporalPgDBConnectorOpen()
{
    char oldPgDbConnCheckSql[] = "user=postgres password=password dbname=orion_ld"; //Need to be changed to environment variables CHANDRA-TBD
    oldPgDbConnection = PQconnectdb(oldPgDbConnCheckSql);
    if (PQstatus(oldPgDbConnection) == CONNECTION_BAD)
    {
        LM_E(("Connection to Postgress database is not achieved"));
        LM_E(("CONNECTION_BAD %s\n", PQerrorMessage(oldPgDbConnection)));
        TemporalPgDBConnectorClose(); //close connection and cleanup
        return false;
    }
    else if (PQstatus(oldPgDbConnection) == CONNECTION_OK)
    {
        //puts("CONNECTION_OK");
        LM_K(("Connection is ok with the Postgres database\n"));
        return true; //Return the connection handler
    }
    return false;
}


// ----------------------------------------------------------------------------
//
// bool TemporalPgDBConnectorOpen(char *tenantName) - function to open the Postgres database connection
//
// ----------------------------------------------------------------------------
bool TemporalPgDBConnectorOpen(char *tenantName)
{
    LM_K(("Trying to open connection to Postgres database for new tenat database creation %s\n", tenantName));

    //  oldPgDbConnection = TemporalDBConnectorOpen();
    if(TemporalPgDBConnectorOpen() != false)
    {
        LM_K(("Trying to create database for Tenant %s\n", tenantName));

        char oldPgDbSqlSyntax[]= ";";
        char oldPgDbSqlCreateTDbSQL[] = "CREATE DATABASE ";
        strcat (oldPgDbSqlCreateTDbSQL, tenantName);
        strcat (oldPgDbSqlCreateTDbSQL, oldPgDbSqlSyntax);
        char oldPgTDbConnSQL[] = "user=postgres password=password dbname= ";
        strcat (oldPgTDbConnSQL, tenantName);

        LM_K(("Command to create database for Tenant %s\n", tenantName));

        PGresult* oldPgTenandDbResult = PQexec(oldPgDbConnection, oldPgDbSqlCreateTDbSQL);
        LM_K(("Opening database connection for Tenant %s\n", tenantName));

        oldPgDbTenantConnection = PQconnectdb(oldPgTDbConnSQL);
        if (PQresultStatus(oldPgTenandDbResult) != PGRES_COMMAND_OK && PQstatus(oldPgDbTenantConnection) != CONNECTION_OK)
        {
                LM_E(("Connection to %s database is not achieved or created", tenantName));
                LM_E(("CONNECTION_BAD %s\n", PQerrorMessage(oldPgDbTenantConnection)));
                TemporalPgDBConnectorClose(); //close Tenant DB connection and cleanup
                return false;
        }
        PQclear(oldPgTenandDbResult);
	}
	else
	{
		 LM_E(("Connection to PostGress database is not achieved or created", tenantName));
                 LM_E(("CONNECTION_BAD %s\n", PQerrorMessage(oldPgDbConnection)));
                 TemporalPgDBConnectorClose(); //close Tenant DB connection and cleanup
	}
	return true;
}


// ----------------------------------------------------------------------------
//
// temporalTenantInitialise -
//
bool temporalTenantInitialise(char *tenantName)
{
    LM_K(("Trying to open connection to Postgres database for new tenat database creation %s\n", tenantName));

    //  oldPgDbConnection = TemporalDBConnectorOpen();
    if(TemporalPgDBConnectorOpen() != false)
    {
        LM_K(("Trying to create database for Tenant %s\n", tenantName));

    		int oldPgDbSqlCreateTDbSQLBufferSize = 1024;
    		int oldPgDbSqlCreateTDbSQLUsedBufferSize = 0;
      	char* oldPgDbSqlCreateTDbSQL = kaAlloc(&orionldState.kalloc, oldPgDbSqlCreateTDbSQLBufferSize);

        // char oldPgDbSqlSyntax[]= ";";
        // char oldPgDbSqlCreateTDbSQL[] = "CREATE DATABASE ";
        // strcat (oldPgDbSqlCreateTDbSQL, tenantName);
        // strcat (oldPgDbSqlCreateTDbSQL, oldPgDbSqlSyntax);
        // char oldPgTDbConnSQL[] = "user=postgres password=orion dbname= ";
        // strcat (oldPgTDbConnSQL, tenantName);


    		strncpy(oldPgDbSqlCreateTDbSQL, "CREATE DATABASE ", oldPgDbSqlCreateTDbSQLBufferSize);
    		oldPgDbSqlCreateTDbSQLUsedBufferSize += 16;
    		strncat(oldPgDbSqlCreateTDbSQL, tenantName, oldPgDbSqlCreateTDbSQLBufferSize - oldPgDbSqlCreateTDbSQLUsedBufferSize);
    		oldPgDbSqlCreateTDbSQLUsedBufferSize += sizeof(tenantName);
        strncpy(oldPgDbSqlCreateTDbSQL, ";", oldPgDbSqlCreateTDbSQLBufferSize - oldPgDbSqlCreateTDbSQLUsedBufferSize);
        oldPgDbSqlCreateTDbSQLUsedBufferSize += 1;

    		int oldPgTDbConnSQLBufferSize = 1024;
    		int oldPgTDbConnSQLUsedBufferSize = 0;
    		char oldPgTDbConnSQLUser[] = "postgres"; // Chandra-TBD
    		char oldPgTDbConnSQLPasswd[] = "orion"; // Chandra-TBD
    		char* oldTemporalSQLBuffer = kaAlloc(&orionldState.kalloc, oldPgTDbConnSQLBufferSize);

        strncpy(oldTemporalSQLBuffer, "user=", oldPgTDbConnSQLBufferSize);
        oldPgTDbConnSQLUsedBufferSize += 5;
    		strncat(oldTemporalSQLBuffer, oldPgTDbConnSQLUser, oldPgTDbConnSQLBufferSize - oldPgTDbConnSQLUsedBufferSize);
    		oldPgTDbConnSQLUsedBufferSize += sizeof(oldPgTDbConnSQLUser);
        strncat(oldTemporalSQLBuffer, " ", oldPgTDbConnSQLBufferSize - oldPgTDbConnSQLUsedBufferSize);
        oldPgTDbConnSQLUsedBufferSize += 1;
        strncat(oldTemporalSQLBuffer, "password=", oldPgTDbConnSQLBufferSize - oldPgTDbConnSQLUsedBufferSize);
        oldPgTDbConnSQLUsedBufferSize += 9;
        strncat(oldTemporalSQLBuffer, oldPgTDbConnSQLPasswd, oldPgTDbConnSQLBufferSize - oldPgTDbConnSQLUsedBufferSize);
        oldPgTDbConnSQLUsedBufferSize += sizeof(oldPgTDbConnSQLPasswd);
        strncat(oldTemporalSQLBuffer, " ", oldPgTDbConnSQLBufferSize - oldPgTDbConnSQLUsedBufferSize);
        oldPgTDbConnSQLUsedBufferSize += 1;
        strncat(oldTemporalSQLBuffer, "dbname=", oldPgTDbConnSQLBufferSize - oldPgTDbConnSQLUsedBufferSize);
        oldPgTDbConnSQLUsedBufferSize += 7;
        strncat(oldTemporalSQLBuffer, tenantName, oldPgTDbConnSQLBufferSize - oldPgTDbConnSQLUsedBufferSize);
        oldPgTDbConnSQLUsedBufferSize += sizeof(tenantName);


        LM_K(("Command to create database for Tenant %s\n", tenantName));

        PGresult* oldPgTenandDbResult = PQexec(oldPgDbConnection, oldPgDbSqlCreateTDbSQL);
        LM_K(("Opening database connection for Tenant %s\n", tenantName));

        oldPgDbTenantConnection = PQconnectdb(oldTemporalSQLBuffer);
        if (PQresultStatus(oldPgTenandDbResult) != PGRES_COMMAND_OK && PQstatus(oldPgDbTenantConnection) != CONNECTION_OK)
        {
            LM_E(("Connection to %s database is not achieved or created", tenantName));
            LM_E(("CONNECTION_BAD %s\n", PQerrorMessage(oldPgDbTenantConnection)));
            TemporalPgDBConnectorClose(); //close Tenant DB connection and cleanup
            return false;
        }
        else if (PQstatus(oldPgDbTenantConnection) == CONNECTION_OK)
        {
            LM_K(("Connection is ok with the %s database\n", tenantName));
            LM_K(("Now crreating the tables for the teanant %s \n", tenantName));
            const char *oldPgDbCreateTenantTables[9][250] =
            {
                "CREATE EXTENSION IF NOT EXISTS postgis",

                //  "CREATE EXTENSION IF NOT EXISTS timescaledb",

                "drop table attribute_sub_properties_table",

                "drop table attributes_table",

                "drop type attribute_value_type_enum",

                "drop table entity_table",

                "CREATE TABLE IF NOT EXISTS entity_table (entity_id TEXT NOT NULL,"
                "entity_type TEXT, geo_property GEOMETRY,created_at TIMESTAMP,"
                "modified_at TIMESTAMP, observed_at TIMESTAMP,PRIMARY KEY (entity_id)),",

                "create type attribute_value_type_enum as enum ('value_string',"
                "'value_number', 'value_boolean', 'value_relation',"
                "'value_object', 'value_datetime', 'value_geo')",

                "CREATE TABLE IF NOT EXISTS attributes_table"
                "(entity_id TEXT NOT NULL REFERENCES entity_table(entity_id),"
                "id TEXT NOT NULL, name TEXT, value_type attribute_value_type_enum,"
                "sub_property BOOL, unit_code TEXT, data_set_id TEXT,"
                "instance_id TEXT NOT NULL, value_string TEXT,"
                "value_boolean BOOL, value_number float8,"
                "value_relation TEXT,value_object TEXT, value_datetime TIMESTAMP,"
                "geo_property GEOMETRY,created_at TIMESTAMP NOT NULL,"
                "modified_at TIMESTAMP NOT NULL,observed_at TIMESTAMP NOT NULL,"
                "PRIMARY KEY (entity_id,id,observed_at,created_at,modified_at))",

                //  "SELECT create_hypertable('attributes_table', 'modified_at')",

                "CREATE TABLE IF NOT EXISTS attribute_sub_properties_table"
                "(entity_id TEXT NOT NULL,attribute_id TEXT NOT NULL,"
                "attribute_instance_id TEXT NOT NULL, id TEXT NOT NULL,"
                "value_type attribute_value_type_enum,value_string TEXT,"
                "value_boolean BOOL, value_number float8, "
                "value_relation TEXT,name TEXT,geo_property GEOMETRY,"
                "unit_code TEXT, value_object TEXT, value_datetime TIMESTAMP,"
                "instance_id bigint GENERATED BY DEFAULT AS IDENTITY"
                "(START WITH 1 INCREMENT BY 1),PRIMARY KEY (instance_id))"
            };
            PQclear(oldPgTenandDbResult);

            for(int oldPgDbNumObj = 0; oldPgDbNumObj < 11; oldPgDbNumObj++)
            {
                oldPgTenandDbResult = PQexec(oldPgDbTenantConnection, *oldPgDbCreateTenantTables[oldPgDbNumObj]);

                if (PQresultStatus(oldPgTenandDbResult) != PGRES_COMMAND_OK)
                {
                        LM_K(("Postgres DB command failed for database for Tenant %s%s\n", tenantName,oldPgDbCreateTenantTables[oldPgDbNumObj]));
                        break;
                }
                PQclear(oldPgTenandDbResult);
            }
        }
    }
    else
    {
            TemporalPgDBConnectorClose(); //close Postgres DB connection and cleanup
            return false;
    }
    return true;
}

// -----------------------------------------------------------------------------
//
// temporalExecSqlStatement
//
//
bool temporalExecSqlStatement(char* oldTemporalSQLBuffer)
{
        char oldTenantName[] = "orion_ld";

        TemporalPgDBConnectorOpen(oldTenantName);  //  opening Tenant Db connection

        oldPgTenandDbResult = PQexec(oldPgDbTenantConnection, "BEGIN");
        if (PQresultStatus(oldPgTenandDbResult) != PGRES_COMMAND_OK)
        {
                LM_E(("BEGIN command failed for inserting single Entity into DB %s\n",oldTenantName));
                PQclear(oldPgTenandDbResult);
                TemporalPgDBConnectorClose();
                return false;
        }
        PQclear(oldPgTenandDbResult);

	//  char* oldTemporalSQLFullBuffer = temporalCommonExtractTree();
	oldPgTenandDbResult = PQexec(oldPgDbTenantConnection, oldTemporalSQLBuffer);
	if (PQresultStatus(oldPgTenandDbResult) != PGRES_COMMAND_OK)
  	{
        	LM_E(("%s command failed for inserting single Attribute into DB %s",oldTemporalSQLBuffer, oldTenantName));
          LM_E(("Reason %s",PQerrorMessage(oldPgDbTenantConnection)));
        	PQclear(oldPgTenandDbResult);
        	TemporalPgDBConnectorClose();
        	return false;
  	}
  	PQclear(oldPgTenandDbResult);


	oldPgTenandDbResult = PQexec(oldPgDbTenantConnection, "COMMIT");
  	if (PQresultStatus(oldPgTenandDbResult) != PGRES_COMMAND_OK)
  	{
        	LM_E(("COMMIT command failed for inserting single Sub Attribute into DB %s\n",oldTenantName));
        	PQclear(oldPgTenandDbResult);
        	TemporalPgDBConnectorClose();
        	return false;
  	}

  	PQclear(oldPgTenandDbResult);
  	TemporalPgDBConnectorClose();
  	return true;
}


// -----------------------------------------------------------------------------
//
// numberToDate -
//
static bool numberToDate(double timestamp, char* date, int dateLen)
{
  struct tm  tm;
  time_t fromEpoch = timestamp;
  //int milliSec = (timestamp - fromEpoch) * 1000;

  gmtime_r(&fromEpoch, &tm);
  strftime(date, dateLen, "%Y-%m-%dT%H:%M:%S", &tm);

  return true;
}

// ----------------------------------------------------------------------------
//
// PGconn* TemporalConstructUpdateSQLStatement(OrionldTemporalDbAllTables* dbAllTablesLocal) - function to buil update SQL statement
//
// ----------------------------------------------------------------------------
bool TemporalConstructInsertSQLStatement(OrionldTemporalDbAllTables* dbAllTablesLocal, bool entityUpdateFlag)
{
    //if (strcmp (tableName,"Entity") == 0);
    //int temporalSQLStatementLengthBuffer = sizeof(dbAllTablesLocal->dbEntityTableLocal);
    //char* updateEntityTableSQLStatement = temporalSQLStatementLengthBuffer * 1024;  // Not smart Chandra-TBI
    //int dbEntityTable = sizeof(dbAllTablesLocal.entityTableArray);
    int dbEntityTable = dbAllTablesLocal->entityTableArrayItems;
    int dbAttribTable = dbAllTablesLocal->attributeTableArrayItems;
    int dbSubAttribTable = dbAllTablesLocal->subAttributeTableArrayItems;


    int dbEntityBufferSize = 10 * 1024;
    char* dbEntityStrBuffer = kaAlloc(&orionldState.kalloc, dbEntityBufferSize);
    bzero(dbEntityStrBuffer, dbEntityBufferSize);

    for (int dbEntityLoop=0; dbEntityLoop < dbEntityTable; dbEntityLoop++)
    {
        char* expandedEntityType = orionldContextItemExpand(orionldState.contextP,
          dbAllTablesLocal->entityTableArray[dbEntityLoop].entityType, NULL, true, NULL);

        char entCreatedAt[64];
        char entModifiedAt[64];

        numberToDate (dbAllTablesLocal->entityTableArray[dbEntityLoop].createdAt,
              entCreatedAt, sizeof(entCreatedAt));

        numberToDate (dbAllTablesLocal->entityTableArray[dbEntityLoop].modifiedAt,
              entModifiedAt, sizeof(entModifiedAt));

        if(entityUpdateFlag)
        {
          snprintf(dbEntityStrBuffer, dbEntityBufferSize, "UPDATE entity_table "
                "SET created_at = '%s', modified_at = '%s' WHERE entity_id = '%s'",
                //dbAllTablesLocal->entityTableArray[dbEntityLoop].createdAt,
                //dbAllTablesLocal->entityTableArray[dbEntityLoop].modifiedAt,
                entCreatedAt, entModifiedAt,
                dbAllTablesLocal->entityTableArray[dbEntityLoop].entityId);
        }
        else
        {
          snprintf(dbEntityStrBuffer, dbEntityBufferSize, "INSERT INTO entity_table(entity_id,entity_type,geo_property,"
                "created_at,modified_at, observed_at) VALUES ('%s', '%s', NULL, '%s', '%s', NULL)",
                dbAllTablesLocal->entityTableArray[dbEntityLoop].entityId,
                expandedEntityType,
                //dbAllTablesLocal->entityTableArray[dbEntityLoop].createdAt,
                //dbAllTablesLocal->entityTableArray[dbEntityLoop].modifiedAt
                entCreatedAt, entModifiedAt);
        }
        //
        // Some traces just to see how the KjNode tree works
        //
        LM_TMP(("CCSR: dbEntityStrBuffer:     '%s'", dbEntityStrBuffer));
        LM_TMP(("CCSR:"));
    }

    if(!temporalExecSqlStatement (dbEntityStrBuffer))
      return false;

    //  temporalExecSqlStatement (dbEntityStrBuffer);  //Chandra - hack TBR

    for (int dbAttribLoop=0; dbAttribLoop < dbAttribTable; dbAttribLoop++)
    {
        LM_TMP(("CCSR: dbAttribLoop:     '%i'", dbAttribLoop));
        int dbAttribBufferSize = 10 * 1024;
        char* dbAttribStrBuffer = kaAlloc(&orionldState.kalloc, dbAttribBufferSize);
        bzero(dbAttribStrBuffer, dbAttribBufferSize);

        int allValuesSize = 2048;
        char* allValues = kaAlloc(&orionldState.kalloc,allValuesSize);

        allValuesRenderAttr (&dbAllTablesLocal->attributeTableArray[dbAttribLoop], allValues, allValuesSize);

            //Chandra-TBI

        char* uuidBuffer = kaAlloc(&orionldState.kalloc, 64);
        uuidGenerate(uuidBuffer);

        char attrCreatedAt[64];
        char attrModifiedAt[64];
        char attrObservedAt[64];

        numberToDate (dbAllTablesLocal->attributeTableArray[dbAttribLoop].createdAt,
              attrCreatedAt, sizeof(attrCreatedAt));

        numberToDate (dbAllTablesLocal->attributeTableArray[dbAttribLoop].modifiedAt,
              attrModifiedAt, sizeof(attrModifiedAt));

        numberToDate (dbAllTablesLocal->attributeTableArray[dbAttribLoop].observedAt,
              attrObservedAt, sizeof(attrObservedAt));


        char* expandedAttrType = orionldContextItemExpand(orionldState.contextP,
                dbAllTablesLocal->attributeTableArray[dbAttribLoop].attributeType, NULL, true, NULL);

        LM_TMP (("CCSR - Printing attributeName %s", dbAllTablesLocal->attributeTableArray[dbAttribLoop].attributeName));

          //"type,", Fix me - put type back Chandra TBC
        snprintf(dbAttribStrBuffer, dbAttribBufferSize, "INSERT INTO attributes_table(entity_id,id,"
            "name, value_type,"
            "sub_property,instance_id, unit_code, data_set_id, value_string,"
            "value_boolean, value_number, value_relation,"
            "value_object, value_datetime, geo_property, "
            "created_at, modified_at, observed_at) "
                "VALUES ('%s', '%s', '%s', '%s', '%s', '%s', %s, '%s', '%s', '%s')",
                dbAllTablesLocal->attributeTableArray[dbAttribLoop].entityId,
                dbAllTablesLocal->attributeTableArray[dbAttribLoop].attributeName,
                expandedAttrType,
                dbValueEnumString(dbAllTablesLocal->attributeTableArray[dbAttribLoop].attributeValueType),  //Chandra-TBD
                (dbAllTablesLocal->attributeTableArray[dbAttribLoop].subProperty==true)? "true" : "false",
                uuidBuffer, allValues, attrCreatedAt, attrModifiedAt, attrObservedAt);

        LM_TMP (("CCSR - Printing SQL attribute %s", dbAttribStrBuffer));
        if(!temporalExecSqlStatement (dbAttribStrBuffer))
          return false;

        for (int dbSubAttribLoop=0; dbSubAttribLoop < dbSubAttribTable; dbSubAttribLoop++)
        {
            int dbSubAttribBufferSize = 10 * 1024;
            char* dbSubAttribStrBuffer = kaAlloc(&orionldState.kalloc, dbSubAttribBufferSize);
            bzero(dbSubAttribStrBuffer, dbSubAttribBufferSize);
            //Chandra-TBI

            int allValuesSizeSubAttr = 2048;
            char* allValuesSubAttr = kaAlloc(&orionldState.kalloc,allValuesSizeSubAttr);

            allValuesRenderSubAttr (&dbAllTablesLocal->subAttributeTableArray[dbSubAttribLoop], allValuesSubAttr, allValuesSizeSubAttr);

            char subAttrCreatedAt[64];
            char subAttrModifiedAt[64];
            char subAttrObeservedAt[64];

            numberToDate (dbAllTablesLocal->attributeTableArray[dbSubAttribLoop].createdAt,
                  subAttrCreatedAt, sizeof(subAttrCreatedAt));

            numberToDate (dbAllTablesLocal->attributeTableArray[dbSubAttribLoop].modifiedAt,
                  subAttrModifiedAt, sizeof(subAttrModifiedAt));

            numberToDate (dbAllTablesLocal->subAttributeTableArray[dbSubAttribLoop].observedAt,
                  subAttrObeservedAt, sizeof(subAttrObeservedAt));

            snprintf(dbSubAttribStrBuffer, dbSubAttribBufferSize, "INSERT INTO attribute_sub_properties_table(entity_id,"
                    " attribute_id, id, type, value_type, unit_code, data_set_id, value_string, value_boolean, value_number,"
                    "value_relation,value_object, value_datetime, geo_property, observed_at, created_at, modified_at)"
                    "VALUES ('%s', '%s', '%s', '%s', '%s', '%s', '%s', '%s', '%s', '%s')",
                    dbAllTablesLocal->subAttributeTableArray[dbSubAttribLoop].entityId,
                    dbAllTablesLocal->subAttributeTableArray[dbSubAttribLoop].attributeName,
                    dbAllTablesLocal->subAttributeTableArray[dbSubAttribLoop].subAttributeName,
                    dbAllTablesLocal->subAttributeTableArray[dbSubAttribLoop].subAttributeType,
                    dbValueEnumString(dbAllTablesLocal->subAttributeTableArray[dbSubAttribLoop].subAttributeValueType),  //Chandra-TBD
                    //(dbAllTablesLocal->subAttributeTableArray[dbSubAttribLoop].subProperty==true)? "true" : "false",
                    uuidBuffer, allValuesSubAttr, subAttrCreatedAt, subAttrModifiedAt, subAttrObeservedAt);
        }
    }
    return true;
}

void allValuesRenderAttr (OrionldTemporalDbAttributeTable* attrLocalP, char* allValues, int allValuesSize)
{
    LM_TMP(("Temporal allValuesRenderAttr - attrLocalP->attributeValueType %i",attrLocalP->attributeValueType));

    char attributeValue[512];

    switch (attrLocalP->attributeValueType)
    {
      case EnumValueString:
        snprintf(attributeValue, sizeof(attributeValue), "'%s', NULL, NULL, NULL, NULL",attrLocalP->valueString);
        LM_TMP (("Printing all Values in attribute extract %s", attributeValue));
        break;

        case EnumValueBool:
          snprintf(attributeValue, sizeof(attributeValue), "NULL, '%s', NULL, NULL, NULL",(attrLocalP->valueBool==true)? "true" : "false");
          break;

        case EnumValueNumber:
          snprintf(attributeValue, sizeof(attributeValue), "NULL, NULL, %lld, NULL, NULL",attrLocalP->valueNumber);
          break;

        case EnumValueRelation:  // same object
          snprintf(attributeValue, sizeof(attributeValue), "NULL, NULL, NULL, '%s', NULL",attrLocalP->valueString);
          break;

        case EnumValueArray:  // same object
          snprintf(attributeValue, sizeof(attributeValue), "NULL, NULL, NULL, '%s', NULL",attrLocalP->valueString);
          break;

        case EnumValueObject:
          snprintf(attributeValue, sizeof(attributeValue), "NULL, NULL, NULL, '%s', NULL",attrLocalP->valueString);
          break;

        case EnumValueDateTime:
          char atrrValueDataTime[64];
          numberToDate (attrLocalP->valueDatetime, atrrValueDataTime, sizeof(atrrValueDataTime));
          snprintf(attributeValue, sizeof(attributeValue), "NULL, NULL, NULL, NULL, '%s'",atrrValueDataTime);
          break;

        default:
          LM_W(("Error - Invalid attribute Value type %d", attrLocalP->attributeValueType));
          return;
    }

    int unitCodeValuesSize = 128;
    char* unitCodeValue = kaAlloc(&orionldState.kalloc, unitCodeValuesSize);
    bzero(unitCodeValue, unitCodeValuesSize);

    if (attrLocalP->unitCode == NULL)
    {
        snprintf(unitCodeValue, unitCodeValuesSize, "NULL");
    }
    else
    {
        snprintf(unitCodeValue, unitCodeValuesSize, "%s", attrLocalP->unitCode);
    }

    int dataSetIdSize = 128;
    char* dataSetIdValue = kaAlloc(&orionldState.kalloc, dataSetIdSize);
    bzero(dataSetIdValue, dataSetIdSize);

    if (attrLocalP->dataSetId == NULL)
    {
        snprintf(dataSetIdValue, dataSetIdSize, "NULL");
    }
    else
    {
        snprintf(dataSetIdValue, dataSetIdSize, "%s", attrLocalP->dataSetId);
    }

    int geoPropertySize = 512;
    char* geoPropertyValue = kaAlloc(&orionldState.kalloc, geoPropertySize);
    bzero(geoPropertyValue, geoPropertySize);

    if (attrLocalP->geoProperty == NULL)
    {
        snprintf(geoPropertyValue, geoPropertySize, "NULL");
    }
    else
    {
        // Chandra-TBI
        //snprintf(geoPropertyValue, geoPropertySize, "%f", attrLocalP->geoProperty);
        snprintf(geoPropertyValue, geoPropertySize, "%s", "NULL");
    }

    int observedAtSize = 512;
    char* observedAtValue = kaAlloc(&orionldState.kalloc, observedAtSize);
    bzero(observedAtValue, observedAtSize);

    if (attrLocalP->observedAt == 0)  // Chandra - Need to be initialize the value
    {
        snprintf(observedAtValue, observedAtSize, "NULL");
    }
    else
    {
        snprintf(observedAtValue, observedAtSize, "%f", attrLocalP->observedAt);
    }

    //char* uuidBuffer = kaAlloc(&orionldState.kalloc, 64);
    //uuidGenerate(uuidBuffer);

    snprintf(allValues, allValuesSize, "%s, %s, %s, %s, %s",
        unitCodeValue, dataSetIdValue, attributeValue, geoPropertyValue, observedAtValue);

    LM_TMP (("Printing all Values in the end in attribute extract %s", allValues));

}


void allValuesRenderSubAttr (OrionldTemporalDbSubAttributeTable* attrLocalP, char* allValues, int allValuesSize)
{
  char subAttributeValue[512];
    switch (attrLocalP->subAttributeValueType)
    {
      case EnumValueString:
        snprintf(subAttributeValue, sizeof(subAttributeValue), "'%s', NULL, NULL, NULL, NULL",attrLocalP->subAttributeValueString);
        break;

        case EnumValueNumber:
          snprintf(subAttributeValue, sizeof(subAttributeValue), "NULL, %lld, NULL, NULL, NULL",attrLocalP->subAttributeValueNumber);
          break;

        case EnumValueBool:
          snprintf(subAttributeValue, sizeof(subAttributeValue), "NULL, NULL, '%s', NULL, NULL",(attrLocalP->subAttributeValueBoolean==true)? "true" : "false");
          break;

        case EnumValueRelation:  // same object
          snprintf(subAttributeValue, sizeof(subAttributeValue), "NULL, NULL, NULL, '%s', NULL",attrLocalP->subAttributeValueString);
          break;

        case EnumValueArray:
          snprintf(subAttributeValue, sizeof(subAttributeValue), "NULL, NULL, NULL, '%s', NULL",attrLocalP->subAttributeValueString);
          break;

        case EnumValueObject:
          snprintf(subAttributeValue, sizeof(subAttributeValue), "NULL, NULL, NULL, '%s', NULL",attrLocalP->subAttributeValueString);
          break;

        case EnumValueDateTime:
          char subAtrrValueDataTime[64];
          numberToDate (attrLocalP->subAttributeValueDatetime, subAtrrValueDataTime, sizeof(subAtrrValueDataTime));
          snprintf(subAtrrValueDataTime, sizeof(subAtrrValueDataTime), "NULL, NULL, NULL, NULL, '%s'",subAtrrValueDataTime);
          break;

        default:
          LM_W(("Error - Invalid Sub attribute Value type %d", attrLocalP->subAttributeValueType));
          return;
    }

    int unitCodeValuesSize = 128;
    char* unitCodeValue = kaAlloc(&orionldState.kalloc, unitCodeValuesSize);
    bzero(unitCodeValue, unitCodeValuesSize);

    if (attrLocalP->subAttributeUnitCode == NULL)
    {
        snprintf(unitCodeValue, unitCodeValuesSize, "NULL");
    }
    else
    {
        snprintf(unitCodeValue, unitCodeValuesSize, "%s", attrLocalP->subAttributeUnitCode);
    }

    int dataSetIdSize = 128;
    char* dataSetIdValue = kaAlloc(&orionldState.kalloc, dataSetIdSize);
    bzero(dataSetIdValue, dataSetIdSize);

    if (attrLocalP->subAttributeDataSetId == NULL)
    {
        snprintf(dataSetIdValue, dataSetIdSize, "NULL");
    }
    else
    {
        snprintf(dataSetIdValue, dataSetIdSize, "%s", attrLocalP->subAttributeDataSetId);
    }

    int geoPropertySize = 512;
    char* geoPropertyValue = kaAlloc(&orionldState.kalloc, geoPropertySize);
    bzero(geoPropertyValue, geoPropertySize);

    if (attrLocalP->subAttributeGeoProperty == NULL)
    {
        snprintf(geoPropertyValue, geoPropertySize, "NULL");
    }
    else
    {
        // Chandra-TBI
        // snprintf(geoPropertyValue, geoPropertySize, "%f", attrLocalP->geoProperty);
    }

    int observedAtSize = 512;
    char* observedAtValue = kaAlloc(&orionldState.kalloc, observedAtSize);
    bzero(observedAtValue, observedAtSize);

    if (attrLocalP->observedAt == 0)  // Chandra - Need to be initialize the value
    {
        snprintf(observedAtValue, observedAtSize, "NULL");
    }
    else
    {
        snprintf(observedAtValue, observedAtSize, "%f", attrLocalP->observedAt);
    }

    char* uuidBuffer = kaAlloc(&orionldState.kalloc, 64);
    uuidGenerate(uuidBuffer);

    snprintf(allValues, allValuesSize, "%s, %s, %s, %s, %s",
        unitCodeValue, dataSetIdValue, allValues, geoPropertyValue, observedAtValue);
}



// ----------------------------------------------------------------------------
//
// PGconn* TemporalPgTenantDBConnectorOpen(char* tenantName) - function to open the Postgres database connection
//
// ----------------------------------------------------------------------------
bool TemporalPgTenantDBConnectorOpen(char* tenantName)
{
    int oldPgTDbConnSQLBufferSize = 1024;
    int oldPgTDbConnSQLUsedBufferSize = 0;
    char oldPgTDbConnSQLUser[] = "postgres"; // Chandra-TBD
    char oldPgTDbConnSQLPasswd[] = "password"; // Chandra-TBD
    char* oldTemporalSQLBuffer = kaAlloc(&orionldState.kalloc, oldPgTDbConnSQLBufferSize);

    strncpy(oldTemporalSQLBuffer, "user=", oldPgTDbConnSQLBufferSize);
    oldPgTDbConnSQLUsedBufferSize += 5;
    strncat(oldTemporalSQLBuffer, oldPgTDbConnSQLUser, oldPgTDbConnSQLBufferSize - oldPgTDbConnSQLUsedBufferSize);
    oldPgTDbConnSQLUsedBufferSize += sizeof(oldPgTDbConnSQLUser);
    strncat(oldTemporalSQLBuffer, " ", oldPgTDbConnSQLBufferSize - oldPgTDbConnSQLUsedBufferSize);
    oldPgTDbConnSQLUsedBufferSize += 1;
    strncat(oldTemporalSQLBuffer, "password=", oldPgTDbConnSQLBufferSize - oldPgTDbConnSQLUsedBufferSize);
    oldPgTDbConnSQLUsedBufferSize += 9;
    strncat(oldTemporalSQLBuffer, oldPgTDbConnSQLPasswd, oldPgTDbConnSQLBufferSize - oldPgTDbConnSQLUsedBufferSize);
    oldPgTDbConnSQLUsedBufferSize += sizeof(oldPgTDbConnSQLPasswd);
    strncat(oldTemporalSQLBuffer, " ", oldPgTDbConnSQLBufferSize - oldPgTDbConnSQLUsedBufferSize);
    oldPgTDbConnSQLUsedBufferSize += 1;
    strncat(oldTemporalSQLBuffer, "dbname=", oldPgTDbConnSQLBufferSize - oldPgTDbConnSQLUsedBufferSize);
    oldPgTDbConnSQLUsedBufferSize += 7;
    strncat(oldTemporalSQLBuffer, tenantName, oldPgTDbConnSQLBufferSize - oldPgTDbConnSQLUsedBufferSize);
    oldPgTDbConnSQLUsedBufferSize += sizeof(tenantName);

    oldPgDbTenantConnection = PQconnectdb(oldTemporalSQLBuffer);

    if (PQstatus(oldPgDbTenantConnection) == CONNECTION_BAD)
    {
        LM_E(("Connection to Tenant database is not achieved"));
        LM_E(("CONNECTION_BAD %s\n", PQerrorMessage(oldPgDbTenantConnection)));
        TemporalPgDBConnectorClose(); //close connection and cleanup
        return false;
    }
    else if (PQstatus(oldPgDbConnection) == CONNECTION_OK)
    {
        //puts("CONNECTION_OK");
        LM_K(("Connection is ok with the Postgres database\n"));
        return true; //Return the connection handler
    }
    return false;
}