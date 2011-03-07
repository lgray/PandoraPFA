/**
 *  @file   PandoraPFANew/Framework/src/Utilities/FileWriter.cc
 * 
 *  @brief  Implementation of the file writer class.
 * 
 *  $Log: $
 */

#include "Api/PandoraContentApi.h"
#include "Api/PandoraContentApiImpl.h"

#include "Helpers/GeometryHelper.h"

#include "Objects/CaloHit.h"
#include "Objects/OrderedCaloHitList.h"
#include "Objects/Track.h"

#include "Utilities/FileWriter.h"

namespace pandora
{

FileWriter::FileWriter(const pandora::Pandora &pandora, const std::string &fileName, const FileMode fileMode) :
    m_pPandora(&pandora),
    m_containerId(UNKNOWN_CONTAINER)
{
    if (APPEND == fileMode)
    {
        m_fileStream.open(fileName.c_str(), std::ios::out | std::ios::binary | std::ios::app | std::ios::ate);
    }
    else if (OVERWRITE == fileMode)
    {
        m_fileStream.open(fileName.c_str(), std::ios::out | std::ios::binary | std::ios::trunc);
    }
    else
    {
        throw StatusCodeException(STATUS_CODE_INVALID_PARAMETER);
    }

    if (!m_fileStream.is_open() || !m_fileStream.good())
        throw StatusCodeException(STATUS_CODE_FAILURE);

    m_containerPosition = m_fileStream.tellp();
}

//------------------------------------------------------------------------------------------------------------------------------------------

FileWriter::~FileWriter()
{
    m_fileStream.close();
}

//------------------------------------------------------------------------------------------------------------------------------------------

StatusCode FileWriter::WriteGeometry()
{
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteHeader(GEOMETRY));
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteGeometryParameters());
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteFooter());

    return STATUS_CODE_SUCCESS;
}

//------------------------------------------------------------------------------------------------------------------------------------------

StatusCode FileWriter::WriteEvent()
{
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteHeader(EVENT));

    std::string trackListName;
    const TrackList *pTrackList = NULL;
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, m_pPandora->GetPandoraContentApiImpl()->GetCurrentTrackList(pTrackList, trackListName));
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteTrackList(*pTrackList));

    std::string orderedCaloHitListName;
    const OrderedCaloHitList *pOrderedCaloHitList = NULL;
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, m_pPandora->GetPandoraContentApiImpl()->GetCurrentOrderedCaloHitList(pOrderedCaloHitList, orderedCaloHitListName));
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteOrderedCaloHitList(*pOrderedCaloHitList));

    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteFooter());

    return STATUS_CODE_SUCCESS;
}

//------------------------------------------------------------------------------------------------------------------------------------------

StatusCode FileWriter::WriteEvent(const TrackList &trackList, const OrderedCaloHitList &orderedCaloHitList)
{
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteHeader(EVENT));
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteOrderedCaloHitList(orderedCaloHitList));
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteTrackList(trackList));
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteFooter());

    return STATUS_CODE_SUCCESS;
}

//------------------------------------------------------------------------------------------------------------------------------------------

StatusCode FileWriter::WriteHeader(const ContainerId containerId)
{
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteVariable(pandoraFileHash));
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteVariable(containerId));

    m_containerPosition = m_fileStream.tellp();
    const std::ofstream::pos_type dummyContainerSize(0);
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteVariable(dummyContainerSize));

    m_containerId = containerId;

    return STATUS_CODE_SUCCESS;
}

//------------------------------------------------------------------------------------------------------------------------------------------

StatusCode FileWriter::WriteFooter()
{
    if ((EVENT != m_containerId) && (GEOMETRY != m_containerId))
        return STATUS_CODE_FAILURE;

    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteVariable((EVENT == m_containerId) ? EVENT_END : GEOMETRY_END));
    m_containerId = UNKNOWN_CONTAINER;

    const std::ofstream::pos_type containerSize(m_fileStream.tellp() - m_containerPosition);
    m_fileStream.seekp(m_containerPosition, std::ios::beg);

    if (!m_fileStream.good())
        return STATUS_CODE_FAILURE;

    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteVariable(containerSize));
    m_fileStream.seekp(0, std::ios::end);

    if (!m_fileStream.good())
        return STATUS_CODE_FAILURE;

    m_containerPosition = m_fileStream.tellp();

    return STATUS_CODE_SUCCESS;
}

//------------------------------------------------------------------------------------------------------------------------------------------

StatusCode FileWriter::WriteGeometryParameters()
{
    if (GEOMETRY != m_containerId)
        return STATUS_CODE_FAILURE;

    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteSubDetector(&(GeometryHelper::GetInDetBarrelParameters())));
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteSubDetector(&(GeometryHelper::GetInDetEndCapParameters())));
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteSubDetector(&(GeometryHelper::GetECalBarrelParameters())));
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteSubDetector(&(GeometryHelper::GetECalEndCapParameters())));
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteSubDetector(&(GeometryHelper::GetHCalBarrelParameters())));
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteSubDetector(&(GeometryHelper::GetHCalEndCapParameters())));
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteSubDetector(&(GeometryHelper::GetMuonBarrelParameters())));
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteSubDetector(&(GeometryHelper::GetMuonEndCapParameters())));

    try
    {
        const float mainTrackerInnerRadius(GeometryHelper::GetMainTrackerInnerRadius());
        const float mainTrackerOuterRadius(GeometryHelper::GetMainTrackerOuterRadius());
        const float mainTrackerZExtent(GeometryHelper::GetMainTrackerZExtent());
        PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteVariable(true));
        PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteVariable(mainTrackerInnerRadius));
        PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteVariable(mainTrackerOuterRadius));
        PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteVariable(mainTrackerZExtent));
    }
    catch (StatusCodeException &)
    {
        PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteVariable(false));
    }

    try
    {
        const float coilInnerRadius(GeometryHelper::GetCoilInnerRadius());
        const float coilOuterRadius(GeometryHelper::GetCoilOuterRadius());
        const float coilZExtent(GeometryHelper::GetCoilZExtent());
        PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteVariable(true));
        PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteVariable(coilInnerRadius));
        PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteVariable(coilOuterRadius));
        PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteVariable(coilZExtent));
    }
    catch (StatusCodeException &)
    {
        PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteVariable(false));
    }

    // Additional subdetectors

    return STATUS_CODE_SUCCESS;
}

//------------------------------------------------------------------------------------------------------------------------------------------

StatusCode FileWriter::WriteSubDetector(const GeometryHelper::SubDetectorParameters *const pSubDetectorParameters)
{
    if (GEOMETRY != m_containerId)
        return STATUS_CODE_FAILURE;

    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteVariable(SUB_DETECTOR));

    const bool isInitialized(pSubDetectorParameters->IsInitialized());
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteVariable(isInitialized));

    if (!isInitialized)
        return STATUS_CODE_SUCCESS;

    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteVariable(pSubDetectorParameters->GetInnerRCoordinate()));
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteVariable(pSubDetectorParameters->GetInnerZCoordinate()));
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteVariable(pSubDetectorParameters->GetInnerPhiCoordinate()));
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteVariable(pSubDetectorParameters->GetInnerSymmetryOrder()));
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteVariable(pSubDetectorParameters->GetOuterRCoordinate()));
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteVariable(pSubDetectorParameters->GetOuterZCoordinate()));
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteVariable(pSubDetectorParameters->GetOuterPhiCoordinate()));
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteVariable(pSubDetectorParameters->GetOuterSymmetryOrder()));
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteVariable(pSubDetectorParameters->IsMirroredInZ()));

    const unsigned int nLayers(pSubDetectorParameters->GetNLayers());
    const GeometryHelper::LayerParametersList &layerParametersList(pSubDetectorParameters->GetLayerParametersList());

    if (layerParametersList.size() != nLayers)
        return STATUS_CODE_FAILURE;

    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteVariable(nLayers));

    for (unsigned int iLayer = 0; iLayer < nLayers; ++iLayer)
    {
        PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteVariable(layerParametersList[iLayer].m_closestDistanceToIp));
        PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteVariable(layerParametersList[iLayer].m_nRadiationLengths));
        PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteVariable(layerParametersList[iLayer].m_nInteractionLengths));
    }

    return STATUS_CODE_SUCCESS;
}

//------------------------------------------------------------------------------------------------------------------------------------------

StatusCode FileWriter::WriteCaloHit(const CaloHit *const pCaloHit)
{
    if (EVENT != m_containerId)
        return STATUS_CODE_FAILURE;

    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteVariable(CALO_HIT));
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteVariable(pCaloHit->GetPositionVector()));
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteVariable(pCaloHit->GetExpectedDirection()));
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteVariable(pCaloHit->GetCellNormalVector()));
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteVariable(pCaloHit->GetCellSizeU()));
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteVariable(pCaloHit->GetCellSizeV()));
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteVariable(pCaloHit->GetCellThickness()));
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteVariable(pCaloHit->GetNCellRadiationLengths()));
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteVariable(pCaloHit->GetNCellInteractionLengths()));
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteVariable(pCaloHit->GetNRadiationLengthsFromIp()));
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteVariable(pCaloHit->GetNInteractionLengthsFromIp()));
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteVariable(pCaloHit->GetTime()));
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteVariable(pCaloHit->GetInputEnergy()));
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteVariable(pCaloHit->GetMipEquivalentEnergy()));
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteVariable(pCaloHit->GetElectromagneticEnergy()));
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteVariable(pCaloHit->GetHadronicEnergy()));
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteVariable(pCaloHit->IsDigital()));
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteVariable(pCaloHit->GetHitType()));
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteVariable(pCaloHit->GetDetectorRegion()));
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteVariable(pCaloHit->GetLayer()));
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteVariable(pCaloHit->IsInOuterSamplingLayer()));
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteVariable(pCaloHit->GetParentCaloHitAddress()));

    return STATUS_CODE_SUCCESS;
}

//------------------------------------------------------------------------------------------------------------------------------------------

StatusCode FileWriter::WriteTrack(const Track *const pTrack)
{
    if (EVENT != m_containerId)
        return STATUS_CODE_FAILURE;

    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteVariable(TRACK));
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteVariable(pTrack->GetD0()));
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteVariable(pTrack->GetZ0()));
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteVariable(pTrack->GetParticleId()));
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteVariable(pTrack->GetCharge()));
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteVariable(pTrack->GetMass()));
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteVariable(pTrack->GetMomentumAtDca()));
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteVariable(pTrack->GetTrackStateAtStart()));
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteVariable(pTrack->GetTrackStateAtEnd()));
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteVariable(pTrack->GetTrackStateAtCalorimeter()));
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteVariable(pTrack->GetTimeAtCalorimeter()));
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteVariable(pTrack->ReachesCalorimeter()));
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteVariable(pTrack->IsProjectedToEndCap()));
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteVariable(pTrack->CanFormPfo()));
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteVariable(pTrack->CanFormClusterlessPfo()));
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteVariable(pTrack->GetParentTrackAddress()));

    return STATUS_CODE_SUCCESS;
}

//------------------------------------------------------------------------------------------------------------------------------------------

StatusCode FileWriter::WriteTrackList(const TrackList &trackList)
{
    for (TrackList::const_iterator iter = trackList.begin(), iterEnd = trackList.end(); iter != iterEnd; ++iter)
    {
        PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteTrack(*iter));
    }

    return STATUS_CODE_SUCCESS;
}

//------------------------------------------------------------------------------------------------------------------------------------------

StatusCode FileWriter::WriteOrderedCaloHitList(const OrderedCaloHitList &orderedCaloHitList)
{
    for (OrderedCaloHitList::const_iterator iter = orderedCaloHitList.begin(), iterEnd = orderedCaloHitList.end(); iter != iterEnd; ++iter)
    {
        for (CaloHitList::const_iterator hitIter = iter->second->begin(), hitIterEnd = iter->second->end(); hitIter != hitIterEnd; ++hitIter)
        {
            PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteCaloHit(*hitIter));
        }
    }

    return STATUS_CODE_SUCCESS;
}

} // namespace pandora