/*
 * @file opencog/embodiment/Control/OperationalAvatarController/PsiRelationUpdaterAgent.cc
 *
 * @author Zhenhua Cai <czhedu@gmail.com>
 * @date 2011-03-03
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License v3 as
 * published by the Free Software Foundation and including the exceptions
 * at http://opencog.org/wiki/Licenses
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program; if not, write to:
 * Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "OAC.h"
#include "PsiRelationUpdaterAgent.h"
#include "PsiRuleUtil.h"

#include<boost/tokenizer.hpp>

using namespace OperationalAvatarController;

extern int currentDebugLevel;

PsiRelationUpdaterAgent::~PsiRelationUpdaterAgent()
{

}

PsiRelationUpdaterAgent::PsiRelationUpdaterAgent()
{
    this->cycleCount = 0;

    // Force the Agent initialize itself during its first cycle. 
    this->forceInitNextCycle();
}

void PsiRelationUpdaterAgent::init(opencog::CogServer * server) 
{
    logger().debug( "PsiRelationUpdaterAgent::%s - Initializing the Agent [ cycle = %d ]",
                    __FUNCTION__, 
                    this->cycleCount
                  );

    // Get OAC
    OAC * oac = (OAC *) server;

    // Get AtomSpace
    const AtomSpace & atomSpace = * ( oac->getAtomSpace() );

    // Get petId
//    const std::string & petId = oac->getPet().getPetId();

    // Get relation names from the configuration file
    std::string relationNames = config()["PSI_RELATIONS"];

    boost::tokenizer<> relationNamesTok (relationNames);

    // Process Relations one by one 
    for ( boost::tokenizer<>::iterator iRelationName = relationNamesTok.begin();
          iRelationName != relationNamesTok.end();
          iRelationName ++ ) {

        // Get corresponding PredicateNode 
        Handle hRelationPredicateNode = atomSpace.getHandle(PREDICATE_NODE, *iRelationName);

        if ( hRelationPredicateNode==opencog::Handle::UNDEFINED ) {
            logger().warn("PsiRelationUpdaterAgent::%s - Failed to find PredicateNode for relation '%s' [ cycle = %d]", 
                          __FUNCTION__, 
                          (*iRelationName).c_str(), 
                          this->cycleCount
                         );
            continue; 
        }

        // Get all the EvaluationLink containing hRelationPredicateNode
        std::vector<Handle> relationEvaluationLinkSet;
        
        atomSpace.getHandleSet( back_inserter(relationEvaluationLinkSet), 
                                hRelationPredicateNode,
                                EVALUATION_LINK, 
                                false
                              );  

        if ( relationEvaluationLinkSet.empty() ) {
            logger().warn("PsiRelationUpdaterAgent::%s - Failed to find EvaluationLink for relation '%s' [ cycle = %d]", 
                          __FUNCTION__, 
                          (*iRelationName).c_str(), 
                          this->cycleCount
                         );
            continue; 

        }

        // Process EvaluationLinks one by one
        foreach(Handle hRelationEvaluationLink, relationEvaluationLinkSet) {

            // Get all the ImplicatonLinks containing hRelationEvaluationLink
            std::vector<Handle> relationImplicationLinkSet; 
           
            atomSpace.getHandleSet( back_inserter(relationImplicationLinkSet), 
                                    hRelationEvaluationLink, 
                                    IMPLICATION_LINK, 
                                    false 
                                  );

            // Process ImplicatonLinks one by one
            // If it is a Psi Rule with NULL_ACTION, append it to instantRelationRules
            foreach(Handle hImplicationLink, relationImplicationLinkSet) {

                // Split the Psi Rule into Goal, Action and Preconditions
                Handle hGoalEvaluationLink, hActionExecutionLink, hPreconditionAndLink; 

                if (PsiRuleUtil::splitPsiRule( atomSpace,
                                               hImplicationLink, 
                                               hGoalEvaluationLink, 
                                               hActionExecutionLink,
                                               hPreconditionAndLink
                                             ) ) {

                    // Check if this Psi Rule contains a NULL_ACTION
                    // About NULL_ACTION, please refer to './opencog/embodiment/rules_core.scm'
                    Handle hActionGroundedSchemaNode = atomSpace.getOutgoing(hActionExecutionLink, 0);

                    if ( atomSpace.getName(hActionGroundedSchemaNode) == "DoNothing" ) {

                        this->instantRelationRules.push_back(hImplicationLink);

                        logger().debug("PsiRelationUpdaterAgent::%s - Found an instant (with NULL_ACTION) Psi Rule '%s' for relatin '%s' [ cycle = %d ]", 
                                       __FUNCTION__, 
                                       atomSpace.atomAsString(hImplicationLink).c_str(), 
                                       (*iRelationName).c_str(), 
                                       this->cycleCount
                                      );
                    }// if
                }// if
            }// foreach

        }// foreach

    }// for

    // Avoid initialize during next cycle
    this->bInitialized = true;
}

Handle PsiRelationUpdaterAgent::getRelationEvaluationLink(opencog::CogServer * server, 
                                                          const std::string & relationName,
                                                          Handle petHandle, 
                                                          Handle entityHandle
                                                         )
{
    // Get the AtomSpace
    AtomSpace & atomSpace = * ( server->getAtomSpace() ); 

    // Get the Handle to relation (PredicateNode)
    Handle relationPredicateHandle = atomSpace.getHandle(PREDICATE_NODE, relationName);

    if (relationPredicateHandle == Handle::UNDEFINED) {
        logger().warn( "PsiRelationUpdaterAgent::%s - Failed to find the PredicateNode for relation '%s'. Don't worry it will be created automatically. [ cycle = %d ]", 
                        __FUNCTION__, 
                        relationName.c_str(), 
                        this->cycleCount
                      );

        relationPredicateHandle = AtomSpaceUtil::addNode(atomSpace, PREDICATE_NODE, relationName, false); 
    }

    // Get the Handle to ListLink that contains both pet and entity
    std::vector<Handle> listLinkOutgoing;

    listLinkOutgoing.push_back(petHandle);
    listLinkOutgoing.push_back(entityHandle);

    Handle listLinkHandle = atomSpace.getHandle(LIST_LINK, listLinkOutgoing);

    if (listLinkHandle == Handle::UNDEFINED) {
        logger().debug( "PsiRelationUpdaterAgent::%s - Failed to find the ListLink containing both entity ( id = '%s' ) and pet ( id = '%s' ). Don't worry, it will be created automatically [ cycle = %d ].", 
                        __FUNCTION__, 
                        atomSpace.getName(entityHandle).c_str(),
                        atomSpace.getName(petHandle).c_str(),
                        this->cycleCount
                      );

        AtomSpaceUtil::addLink(atomSpace, LIST_LINK, listLinkOutgoing, false);
    } 

    // Get the Handle to EvaluationLink holding the relation between the pet and the entity
    std::vector<Handle> evaluationLinkOutgoing; 

    evaluationLinkOutgoing.push_back(relationPredicateHandle);
    evaluationLinkOutgoing.push_back(listLinkHandle);

    Handle evaluationLinkHandle = atomSpace.getHandle(EVALUATION_LINK, evaluationLinkOutgoing);

    if (evaluationLinkHandle == Handle::UNDEFINED) {
        logger().debug( "PsiRelationUpdaterAgent::%s - Failed to find the EvaluationLink holding the '%s' relation between the pet ( id = '%s' ) and the entity ( id = '%s' ). Don't worry it would be created automatically [ cycle = %d ].", 
                        __FUNCTION__, 
                        relationName.c_str(), 
                        atomSpace.getName(petHandle).c_str(),
                        atomSpace.getName(entityHandle).c_str(),
                        this->cycleCount
                     );

        AtomSpaceUtil::addLink(atomSpace, EVALUATION_LINK, evaluationLinkOutgoing, false);
    } 


    return evaluationLinkHandle;
}

Handle PsiRelationUpdaterAgent::getEntityHandle(opencog::CogServer * server, const std::string & entityName)
{
    // TODO: What is responsible for creating these handles to entities?

    AtomSpace & atomSpace = * ( server->getAtomSpace() ); 

    std::vector<Handle> entityHandleSet;

    atomSpace.getHandleSet( back_inserter(entityHandleSet),
                            OBJECT_NODE,
                            entityName,
                            true   // Use 'true' here, because OBJECT_NODE is the base class for all the entities
                          );

    if ( entityHandleSet.size() != 1 ) { 
        logger().warn( "PsiRelationUpdaterAgent::%s - The number of entity ( id = '%s' ) registered in AtomSpace should be exactly 1. Got %d [ cycle = %d ]", 
                       __FUNCTION__, 
                       entityName.c_str(), 
                       entityHandleSet.size(), 
                       this->cycleCount
                     );
        return opencog::Handle::UNDEFINED; 
    } 

    return  entityHandleSet[0];
}

void PsiRelationUpdaterAgent::run(opencog::CogServer * server)
{
    this->cycleCount ++;

    logger().debug( "PsiRelationUpdaterAgent::%s - Executing run %d times",
                     __FUNCTION__, 
                     this->cycleCount
                  );

    // Get OAC
    OAC * oac = (OAC *) server;

    // Get rand generator
    RandGen & randGen = oac->getRandGen();

    // Get AtomSpace
    AtomSpace & atomSpace = * ( oac->getAtomSpace() );

    // Get ProcedureInterpreter
    Procedure::ProcedureInterpreter & procedureInterpreter = oac->getProcedureInterpreter();

    // Get Procedure repository
    const Procedure::ProcedureRepository & procedureRepository = oac->getProcedureRepository();

    // Get petId and petName
    const std::string & petName = oac->getPet().getName();
    const std::string & petId = oac->getPet().getPetId(); 

    // Get Handle to the pet
    Handle petHandle = AtomSpaceUtil::getAgentHandle( atomSpace, petId); 

    if ( petHandle == Handle::UNDEFINED ) {
        logger().warn("PsiRelationUpdaterAgent::%s - Failed to get the handle to the pet ( id = '%s' ) [ cycle = %d ]",
                        __FUNCTION__, 
                        petId.c_str(), 
                        this->cycleCount
                     );
        return;
    }

    // Check if map info data is available
    if ( atomSpace.getSpaceServer().getLatestMapHandle() == Handle::UNDEFINED ) {
        logger().warn("PsiRelationUpdaterAgent::%s - There is no map info available yet [ cycle = %d ]", 
                        __FUNCTION__, 
                        this->cycleCount
                     );
        return;
    }

    // Check if the pet spatial info is already received
    if ( !atomSpace.getSpaceServer().getLatestMap().containsObject(petId) ) {
        logger().warn("PsiRelationUpdaterAgent::%s - Pet was not inserted in the space map yet [ cycle = %d ]", 
                      __FUNCTION__, 
                      this->cycleCount
                     );
        return;
    }

    // Decide whether to update relations during this cognitive cycle (controlled by the modulator 'SecuringThreshold')
    float securingThreshold = AtomSpaceUtil::getCurrentModulatorLevel(randGen,
                                                                      atomSpace,
                                                                      SECURING_THRESHOLD_MODULATOR_NAME, 
                                                                      petId
                                                                     );
// TODO: Uncomment the line below once finish testing
//    if ( randGen.randfloat() < securingThreshold ) 
    {
        logger().debug(
                "PsiRelationUpdaterAgent::%s - Skip updating the relations for this cognitive cycle [ cycle = %d ] ", 
                       __FUNCTION__, 
                       this->cycleCount
                      );
        return; 
    }

    // Initialize entity, relation lists etc.
    if ( !this->bInitialized )
        this->init(server);

    // TODO: Deal with 'curious_about' relation

    // Shuffle all the instant Psi Rules about Relation 
    //
    // Note: Why? Because some Relations rely on other Relations, like multi-steps planning.  
    //       For example, 'familiar_with' is the former step before 'know' and 
    //       'curious_about' is the very basic step for other relations.
    //
    //       Randomly shuffle these instant relation rules before processing would give the pet 
    //       the chance to consider multi-step relations in different combinations.  
    std::random_shuffle( this->instantRelationRules.begin(), this->instantRelationRules.end() );

    // This vector has all possible objects/avatars ids to replace wildcard in Psi Rules
    //
    // TODO: We would not use wildcard later, 
    //       and the vector here should be used to replace VariableNode in Psi Rules with ForAllLink
    std::vector<std::string> varBindCandidates;

    // Process instant relation rules one by one 
    foreach(Handle hInstantRelationRule, this->instantRelationRules) {

        // If all the instant relation rules are satisfied, 
        // set the truth value of corresponding EvaluationLinks to true
        if ( PsiRuleUtil::allPreconditionsSatisfied( atomSpace, 
                                                     procedureInterpreter, 
                                                     procedureRepository, 
                                                     hInstantRelationRule, 
                                                     varBindCandidates, 
                                                     randGen
                                                   ) ) {
            
            // Split the Psi Rule into Goal, Action and Preconditions
            Handle hGoalEvaluationLink, hActionExecutionLink, hPreconditionAndLink; 

            PsiRuleUtil::splitPsiRule( atomSpace,
                                       hInstantRelationRule, 
                                       hGoalEvaluationLink, 
                                       hActionExecutionLink,
                                       hPreconditionAndLink
                                     );

            // Get relation name
            Handle hRelationPredicateNode = atomSpace.getOutgoing(hInstantRelationRule, 0);
            std::string relationName = atomSpace.getName(hRelationPredicateNode);

            // Set all the truth value of all the EvaluationLinks containing this relation to true
            foreach(std::string entityId, varBindCandidates) {
                // Get handle to entity
                Handle entityHandle = this->getEntityHandle(server, entityId);

                if ( entityHandle == opencog::Handle::UNDEFINED )
                    continue; 

                Handle hRelationEvaluationLink = this->getRelationEvaluationLink( server, 
                                                                                  relationName,
                                                                                  petHandle, 
                                                                                  entityHandle            
                                                                                );
                // Set the truth value to true
                // TODO: Actually, this not so correct, the truth value should not be a constant (1, 1)
                //       We should give the pet the ability to distinguish the intensity of relation,
                //       such as friend, good friend and best friend
                SimpleTruthValue stvTrue(1, 1);
                atomSpace.setTV(hRelationEvaluationLink, stvTrue); 
                
                logger().debug("PsiRelationUpdaterAgent::%s Updated the trutu value of '%s' [ cycle = %d ]", 
                               __FUNCTION__, 
                               atomSpace.atomAsString(hRelationEvaluationLink).c_str(), 
                               this->cycleCount
                              );

            }// foreach

        }// if 

    }// foreach

}
