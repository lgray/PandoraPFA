/**
 *  @file   PandoraPFANew/src/Algorithms/Monitoring/MCParticlesMonitoringAlgorithm.cc
 * 
 *  @brief  Implementation of an algorithm to monitor the mc particles
 * 
 *  $Log: $
 */

#include "Algorithms/Monitoring/MCParticlesMonitoringAlgorithm.h"

#include "Api/PandoraContentApi.h"

#include "Objects/MCParticle.h"
#include "Objects/CaloHit.h"
#include "Objects/Track.h"

#include <iostream>
#include <iomanip>
#include <assert.h>

using namespace pandora;


//------------------------------------------------------------------------------------------------------------------------------------------

StatusCode MCParticlesMonitoringAlgorithm::Initialize()
{
    m_energy = NULL;
    m_momentumX = NULL;
    m_momentumY = NULL;
    m_momentumZ = NULL;
    m_particleId = NULL;
    m_outerRadius = NULL;
    m_innerRadius = NULL;

    if( !m_monitoringFileName.empty() && !m_treeName.empty() )
    {
        m_energy = new FloatVector();
        m_momentumX = new FloatVector();                     
        m_momentumY = new FloatVector();                     
        m_momentumZ = new FloatVector();                     
        m_particleId = new IntVector();                    
        m_outerRadius = new FloatVector();                   
        m_innerRadius = new FloatVector();                   
    }
    m_eventCounter = 0;

    return STATUS_CODE_SUCCESS;
}

//------------------------------------------------------------------------------------------------------------------------------------------

MCParticlesMonitoringAlgorithm::~MCParticlesMonitoringAlgorithm()
{
    if( !m_monitoringFileName.empty() && !m_treeName.empty() )
    {
//         PANDORA_MONITORING_API(PrintTree(m_treeName));
        PANDORA_MONITORING_API(SaveTree(m_treeName, m_monitoringFileName, "UPDATE" ));
    }
    delete m_energy;
    delete m_momentumX;
    delete m_momentumY;
    delete m_momentumZ;
    delete m_particleId;
    delete m_outerRadius;
    delete m_innerRadius;
}


//------------------------------------------------------------------------------------------------------------------------------------------

StatusCode MCParticlesMonitoringAlgorithm::Run()
{
    MCParticleList mcParticleList;
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, PandoraContentApi::GetMCParticleList(*this, mcParticleList));

    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, FillListOfUsedMCParticles());

    MonitorMCParticleList(mcParticleList);

    ++m_eventCounter;

    return STATUS_CODE_SUCCESS;
}

//------------------------------------------------------------------------------------------------------------------------------------------

StatusCode MCParticlesMonitoringAlgorithm::ReadSettings(const TiXmlHandle xmlHandle)
{
    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle, "MonitoringFileName", m_monitoringFileName));

    m_treeName = "emon";
    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle, "TreeName", m_treeName));
    m_print = true;
    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle, "Print", m_print));
    m_indent = true;
    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle, "Indent", m_indent));
    m_oldRoot = false;
    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle, "ROOT_OLDER_THAN_5_20", m_oldRoot));

    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadVectorOfValues(xmlHandle, "ClusterListNames", m_clusterListNames));


    StringVector mcParticleSelection;
    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadVectorOfValues(xmlHandle, "Selection", mcParticleSelection));


    m_onlyFinal = false;
    m_haveCaloHits = false;
    m_haveTracks = false;

    for( StringVector::iterator itStr = mcParticleSelection.begin(), itStrEnd = mcParticleSelection.end(); itStr != itStrEnd; ++itStr )
    {
        std::string currentString = (*itStr);
        if( currentString == "Final" )
            m_onlyFinal = true;
        else if( currentString == "CalorimeterHits" ) 
            m_haveCaloHits = true;
        else if( currentString == "Tracks" ) 
            m_haveTracks = true;
        else
        {
            std::cout << "<Selection> '" << currentString << "' unknown." << std::endl;
            return STATUS_CODE_NOT_FOUND;
        }
    }


    m_sort = false;
    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle, "Sort", m_sort));

    return STATUS_CODE_SUCCESS;
}



//------------------------------------------------------------------------------------------------------------------------------------------

void MCParticlesMonitoringAlgorithm::MonitorMCParticleList( const MCParticleList& mcParticleList ) 
{

    if( m_print )
        std::cout << "MCParticle monitoring written into tree : " << m_treeName << std::endl;


    // alle MCParticles ausselektieren die in der parent-list eines anderen particles drinnen sind. TODO!!


    m_energy->clear();
    m_momentumX->clear();
    m_momentumY->clear();
    m_momentumZ->clear();
    m_particleId->clear();
    m_outerRadius->clear();
    m_innerRadius->clear();

    typedef std::map<float, int, std::greater<float> > SortIndex;
    SortIndex sortIndex;
    int mcParticleNumber = 0;

    typedef std::vector<const MCParticle*> MCParticleVector;
    MCParticleVector mcParticleVector;

    for( MCParticleList::const_iterator itMc = mcParticleList.begin(), itMcEnd = mcParticleList.end(); itMc != itMcEnd; ++itMc )
    {
        const MCParticle* pMCParticle = (*itMc);
        
        if( TakeMCParticle(pMCParticle) )
        {
            mcParticleVector.push_back(pMCParticle);

            float energy = pMCParticle->GetEnergy();
            sortIndex[energy] = mcParticleNumber;
            m_energy->push_back( energy );
            const CartesianVector& momentum = pMCParticle->GetMomentum();
            m_momentumX->push_back( momentum.GetX() );
            m_momentumY->push_back( momentum.GetY() );
            m_momentumZ->push_back( momentum.GetZ() );
            m_particleId->push_back( pMCParticle->GetParticleId() );
            m_outerRadius->push_back( pMCParticle->GetOuterRadius() );
            m_innerRadius->push_back( pMCParticle->GetInnerRadius() );

            ++mcParticleNumber;
        }
    }

    if( m_sort )
    {
        for( SortIndex::iterator itIdx = sortIndex.begin(), itIdxEnd = sortIndex.end(); itIdx != itIdxEnd; ++itIdx )
        {
            int idx = itIdx->second;

            assert( fabs(itIdx->first - m_energy->at(idx)) < 0.1 );

            m_energy->push_back     ( m_energy->at(idx)      );
            m_momentumX->push_back  ( m_momentumX->at(idx)   );
            m_momentumY->push_back  ( m_momentumY->at(idx)   );
            m_momentumZ->push_back  ( m_momentumZ->at(idx)   );
            m_particleId->push_back ( m_particleId->at(idx)  );
            m_outerRadius->push_back( m_outerRadius->at(idx) );
            m_innerRadius->push_back( m_innerRadius->at(idx) );

            mcParticleVector.push_back( mcParticleVector.at(idx) );
        }
        size_t sortIndexSize = sortIndex.size();
        m_energy->erase( m_energy->begin(), m_energy->begin() + sortIndexSize );
        m_momentumX->erase( m_momentumX->begin(), m_momentumX->begin() + sortIndexSize );
        m_momentumY->erase( m_momentumY->begin(), m_momentumY->begin() + sortIndexSize );
        m_momentumZ->erase( m_momentumZ->begin(), m_momentumZ->begin() + sortIndexSize );
        m_particleId->erase( m_particleId->begin(), m_particleId->begin() + sortIndexSize );
        m_outerRadius->erase( m_outerRadius->begin(), m_outerRadius->begin() + sortIndexSize );
        m_innerRadius->erase( m_innerRadius->begin(), m_innerRadius->begin() + sortIndexSize );

        mcParticleVector.erase( mcParticleVector.begin(), mcParticleVector.begin() + sortIndexSize );
    }
        
    if( m_print )
    {
        for( MCParticleVector::iterator itMc = mcParticleVector.begin(), itMcEnd = mcParticleVector.end(); itMc != itMcEnd; ++itMc )
        {
            const MCParticle* pMcParticle = (*itMc);
            PrintMCParticle(pMcParticle,std::cout);
            std::cout << "Total number of MCPFOs : " << mcParticleNumber << std::endl;
        }
    }


    if( !m_monitoringFileName.empty() && !m_treeName.empty() )
    {
        if( m_oldRoot )
        {
            for( int i = 0; i < mcParticleNumber; ++i )
            {
                PANDORA_MONITORING_API(SetTreeVariable(m_treeName, "number", i ));
                PANDORA_MONITORING_API(SetTreeVariable(m_treeName, "energy", m_energy->at(i) ));
                PANDORA_MONITORING_API(SetTreeVariable(m_treeName, "pX", m_momentumX->at(i) ));
                PANDORA_MONITORING_API(SetTreeVariable(m_treeName, "pY", m_momentumY->at(i) ));
                PANDORA_MONITORING_API(SetTreeVariable(m_treeName, "pZ", m_momentumZ->at(i) ));
                PANDORA_MONITORING_API(SetTreeVariable(m_treeName, "pdg", m_particleId->at(i) ));
                PANDORA_MONITORING_API(SetTreeVariable(m_treeName, "ro", m_outerRadius->at(i) ));
                PANDORA_MONITORING_API(SetTreeVariable(m_treeName, "ri", m_innerRadius->at(i) ));

                PANDORA_MONITORING_API(FillTree(m_treeName));
            }
        }
        else
        {
            PANDORA_MONITORING_API(SetTreeVariable(m_treeName, "energy", m_energy ));
            PANDORA_MONITORING_API(SetTreeVariable(m_treeName, "pX", m_momentumX ));
            PANDORA_MONITORING_API(SetTreeVariable(m_treeName, "pY", m_momentumY ));
            PANDORA_MONITORING_API(SetTreeVariable(m_treeName, "pZ", m_momentumZ ));
            PANDORA_MONITORING_API(SetTreeVariable(m_treeName, "pdg", m_particleId ));
            PANDORA_MONITORING_API(SetTreeVariable(m_treeName, "ro", m_outerRadius ));
            PANDORA_MONITORING_API(SetTreeVariable(m_treeName, "ri", m_innerRadius ));

            PANDORA_MONITORING_API(FillTree(m_treeName));
        }

//         PANDORA_MONITORING_API(PrintTree(m_treeName));
    }
}






//------------------------------------------------------------------------------------------------------------------------------------------

void MCParticlesMonitoringAlgorithm::PrintMCParticle( const MCParticle* mcParticle, std::ostream & o )
{
    static const char* whiteongreen = "\033[1;42m";  // white on green background
    static const char* reset  = "\033[0m";     // reset

    if( m_indent )
    {
        int printDepth = (int)(mcParticle->GetOuterRadius()/100); // this can be changed if the printout doesn't look good
        o << std::setw (printDepth) << " ";
    }
    if( mcParticle->IsRootParticle() )
    {
        o << whiteongreen << "/ROOT/" << reset;
    }

    const CartesianVector& momentum = mcParticle->GetMomentum();
//   o << "[" << mcParticle << "]"
    o << std::setprecision(2);
    o << std::fixed;
    o << " E=" << mcParticle->GetEnergy()
      << std::scientific
      << " px=" << momentum.GetX()
      << " py=" << momentum.GetY()
      << " pz=" << momentum.GetZ()
      << " pid=" << mcParticle->GetParticleId()
      << std::fixed << std::setprecision(1)
      << " r_i=" << mcParticle->GetInnerRadius()
      << " r_o=" << mcParticle->GetOuterRadius();
//     << " uid=" << mcParticle->GetUid();
//    o << " dghtrs: " << mcParticle->GetDaughterList().size();
}



//------------------------------------------------------------------------------------------------------------------------------------------

StatusCode MCParticlesMonitoringAlgorithm::FillListOfUsedMCParticles()
{

    if( m_clusterListNames.empty() )
    {
        if( m_haveCaloHits )
        {
            const OrderedCaloHitList *pOrderedCaloHitList = NULL;
            PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, PandoraContentApi::GetCurrentOrderedCaloHitList(*this, pOrderedCaloHitList));
        
            for( OrderedCaloHitList::const_iterator itLyr = pOrderedCaloHitList->begin(), itLyrEnd = pOrderedCaloHitList->end(); itLyr != itLyrEnd; itLyr++ )
            {
                // int pseudoLayer = itLyr->first;
                CaloHitList::iterator itCaloHit    = itLyr->second->begin();
                CaloHitList::iterator itCaloHitEnd = itLyr->second->end();

                for( ; itCaloHit != itCaloHitEnd; itCaloHit++ )
                {
                    CaloHit* pCaloHit = (*itCaloHit);

                    // fetch the MCParticle
                    const MCParticle* mc = NULL; 
                    pCaloHit->GetMCParticle( mc );

                    if( mc == NULL ) continue; // has to be continue, since sometimes some CalorimeterHits don't have a MCParticle (e.g. noise)

                    m_mcParticleList.insert( mc );
                }
            }
        }


        if( m_haveTracks )
        {
            const TrackList *pTrackList = NULL;
            PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, PandoraContentApi::GetCurrentTrackList(*this, pTrackList));

            // now for the tracks
            for( TrackList::const_iterator itTrack = pTrackList->begin(), itTrackEnd = pTrackList->end(); itTrack != itTrackEnd; ++itTrack )
            {
                Track* pTrack = (*itTrack);

                const MCParticle* mc = NULL;
                pTrack->GetMCParticle( mc );
                if( mc == NULL ) continue; // maybe an error should be thrown here?

                m_mcParticleList.insert( mc );
            }
        }
    }
    else // if( !m_clusterListNames.empty() )
    {
        typedef std::vector<const ClusterList*> ClusterVector;
        ClusterVector clusterListVector;

        for( pandora::StringVector::iterator itClusterName = m_clusterListNames.begin(), itClusterNameEnd = m_clusterListNames.end(); itClusterName != itClusterNameEnd; ++itClusterName )
        {
            const ClusterList* pClusterList = NULL;
            if( STATUS_CODE_SUCCESS == PandoraContentApi::GetClusterList(*this, (*itClusterName), pClusterList))
                clusterListVector.push_back( pClusterList ); // add the cluster list
        }
    
        for( ClusterVector::iterator itClusterList = clusterListVector.begin(), itClusterListEnd = clusterListVector.end(); itClusterList != itClusterListEnd; ++itClusterList )
        {
            const ClusterList* pClusterList = (*itClusterList);
            for( ClusterList::iterator itCluster = pClusterList->begin(), itClusterEnd = pClusterList->end(); itCluster != itClusterEnd; ++itCluster )
            {
                const Cluster* pCluster = (*itCluster);

                if( m_haveCaloHits )
                {
                    const OrderedCaloHitList& pOrderedCaloHitList = pCluster->GetOrderedCaloHitList();
                    for( OrderedCaloHitList::const_iterator itLyr = pOrderedCaloHitList.begin(), itLyrEnd = pOrderedCaloHitList.end(); itLyr != itLyrEnd; itLyr++ )
                    {
                        // int pseudoLayer = itLyr->first;
                        CaloHitList::iterator itCaloHit    = itLyr->second->begin();
                        CaloHitList::iterator itCaloHitEnd = itLyr->second->end();

                        for( ; itCaloHit != itCaloHitEnd; itCaloHit++ )
                        {
                            CaloHit* pCaloHit = (*itCaloHit);

                            // fetch the MCParticle
                            const MCParticle* mc = NULL; 
                            pCaloHit->GetMCParticle( mc );

                            if( mc == NULL ) continue; // has to be continue, since sometimes some CalorimeterHits don't have a MCParticle (e.g. noise)

                            m_mcParticleList.insert( mc );
                        }
                    }
                }

                if( m_haveTracks )
                {
                    const TrackList& pTrackList = pCluster->GetAssociatedTrackList();
                    
                    // now for the tracks
                    for( TrackList::const_iterator itTrack = pTrackList.begin(), itTrackEnd = pTrackList.end(); itTrack != itTrackEnd; ++itTrack )
                    {
                        Track* pTrack = (*itTrack);

                        const MCParticle* mc = NULL;
                        pTrack->GetMCParticle( mc );
                        if( mc == NULL ) continue; // maybe an error should be thrown here?

                        m_mcParticleList.insert( mc );
                    }
                }
            }
        }
    }

    return STATUS_CODE_SUCCESS;
}



//------------------------------------------------------------------------------------------------------------------------------------------

bool MCParticlesMonitoringAlgorithm::TakeMCParticle(const MCParticle* pMCParticle)
{
    if( m_onlyFinal && !pMCParticle->GetDaughterList().empty() )
    {
        return false;
    }

    if( m_haveCaloHits || m_haveTracks )
    {
        ConstMCParticleList::iterator itMc = m_mcParticleList.find( pMCParticle );
        if( itMc == m_mcParticleList.end() )
        {
            return false;
        }
        return true;
    }

    return true;
}