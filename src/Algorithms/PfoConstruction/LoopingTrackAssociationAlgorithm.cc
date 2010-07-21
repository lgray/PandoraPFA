/**
 *  @file   PandoraPFANew/src/Algorithms/PfoConstruction/LoopingTrackAssociationAlgorithm.cc
 * 
 *  @brief  Implementation of the looping track association algorithm class.
 * 
 *  $Log: $
 */

#include "Algorithms/PfoConstruction/LoopingTrackAssociationAlgorithm.h"

#include "Pandora/AlgorithmHeaders.h"

using namespace pandora;

StatusCode LoopingTrackAssociationAlgorithm::Run()
{
    const TrackList *pTrackList = NULL;
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, PandoraContentApi::GetCurrentTrackList(*this, pTrackList));

    TrackVector trackVector(pTrackList->begin(), pTrackList->end());
    std::sort(trackVector.begin(), trackVector.end(), Track::SortByEnergy);

    const ClusterList *pClusterList = NULL;
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, PandoraContentApi::GetCurrentClusterList(*this, pClusterList));

    static const float endCapZPosition(GeometryHelper::GetInstance()->GetECalEndCapParameters().GetInnerZCoordinate());

    // Loop over all unassociated tracks in the current track list
    for (TrackVector::const_iterator iterT = trackVector.begin(), iterTEnd = trackVector.end(); iterT != iterTEnd; ++iterT)
    {
        Track *pTrack = *iterT;

        // Use only unassociated tracks that can be used to form a pfo
        if (pTrack->HasAssociatedCluster() || !pTrack->CanFormPfo())
            continue;

        if (!pTrack->GetDaughterTrackList().empty())
            continue;

        // Use only tracks that reach the ecal endcap, not barrel
        const float trackECalZPosition(pTrack->GetTrackStateAtECal().GetPosition().GetZ());

        if (std::fabs(trackECalZPosition) > endCapZPosition - m_maxEndCapDeltaZ)
            continue;

        // Extract information from the track
        const Helix *const pHelix(pTrack->GetHelixFitAtECal());
        const float helixOmega(pHelix->GetOmega());

        if (0.f == helixOmega)
            continue;

        const float helixRadius(1.f / helixOmega);
        const float helixTanLambda(pHelix->GetTanLambda());
        const float helixPhi0(pHelix->GetPhi0());

        static const float pi_2(0.5f * std::acos(-1.f));
        const float helixXCentre(helixRadius * std::cos(helixPhi0 - pi_2));
        const float helixYCentre(helixRadius * std::sin(helixPhi0 - pi_2));

        const float helixDCosZ(helixTanLambda / std::sqrt(1.f + helixTanLambda * helixTanLambda));
        const float trackEnergy(pTrack->GetEnergyAtDca());

        // Identify best cluster to be associated with this track, using projection of track helix onto endcap
        Cluster *pBestCluster(NULL);
        float minEnergyDifference(std::numeric_limits<float>::max());
        float smallestDeltaR(std::numeric_limits<float>::max());

        for (ClusterList::const_iterator iterC = pClusterList->begin(), iterCEnd = pClusterList->end(); iterC != iterCEnd; ++iterC)
        {
            Cluster *pCluster = *iterC;

            if (!pCluster->GetAssociatedTrackList().empty())
                continue;

            if ((pCluster->GetNCaloHits() < m_minHitsInCluster) || (pCluster->GetOrderedCaloHitList().size() < m_minOccupiedLayersInCluster))
                continue;

            // Demand that cluster starts in first few layers of ecal
            const PseudoLayer innerLayer(pCluster->GetInnerPseudoLayer());

            if (innerLayer > m_maxClusterInnerLayer)
                continue;

            // Ensure that cluster is in same endcap region as track
            const float clusterZPosition(pCluster->GetCentroid(innerLayer).GetZ());

            if (endCapZPosition - std::fabs(clusterZPosition) > m_maxEndCapDeltaZ)
                continue;

            if (clusterZPosition * trackECalZPosition < 0.f)
                continue;

            // Check consistency of track momentum and cluster energy
            const float chi(ReclusterHelper::GetTrackClusterCompatibility(pCluster->GetTrackComparisonEnergy(), trackEnergy));

            if (std::fabs(chi) > m_maxAbsoluteTrackClusterChi)
                continue;

            // Calculate distance of cluster from centre of helix for i) cluster inner layer and ii) first m_nClusterDeltaRLayers layers
            const CartesianVector innerCentroid(pCluster->GetCentroid(innerLayer));

            const float innerLayerDeltaX(innerCentroid.GetX() - helixXCentre);
            const float innerLayerDeltaY(innerCentroid.GetY() - helixYCentre);
            const float innerLayerDeltaR(std::sqrt((innerLayerDeltaX * innerLayerDeltaX) + (innerLayerDeltaY * innerLayerDeltaY)) - std::fabs(helixRadius));
            const float meanDeltaR(this->GetMeanDeltaR(pCluster, helixXCentre, helixYCentre, helixRadius));

            // Check that cluster is sufficiently close to helix path
            const bool isInRangeInner((innerLayerDeltaR < m_maxDeltaR) && (innerLayerDeltaR > m_minDeltaR));
            const bool isInRangeMean((meanDeltaR < m_maxDeltaR) && (meanDeltaR > m_minDeltaR));

            if (!isInRangeInner && !isInRangeMean)
                continue;

            const float deltaR(std::min(std::fabs(innerLayerDeltaR), std::fabs(meanDeltaR))); // ATTN: Changed order of min and fabs here

            // Calculate projected helix direction at endcap
            CartesianVector helixDirection;

            if (0.f != innerLayerDeltaY)
            {
                float helixDCosX((1.f - helixDCosZ * helixDCosZ) / (1.f + ((innerLayerDeltaX * innerLayerDeltaX) / (innerLayerDeltaY * innerLayerDeltaY))));
                helixDCosX = std::sqrt(std::max(helixDCosX, 0.f));

                if (innerLayerDeltaY * helixRadius < 0)
                    helixDCosX *= -1.f;

                helixDirection.SetValues(helixDCosX, -(innerLayerDeltaX / innerLayerDeltaY) * helixDCosX, helixDCosZ);
            }
            else
            {
                float helixDCosY(1.f - helixDCosZ * helixDCosZ);
                helixDCosY = std::sqrt(std::max(helixDCosY, 0.f));

                if (innerLayerDeltaX * helixRadius > 0)
                    helixDCosY *= -1.f;

                helixDirection.SetValues(0.f, helixDCosY, helixDCosZ);
            }
 
            // Calculate direction of first n layers of cluster
            ClusterHelper::ClusterFitResult clusterFitResult;
            if (STATUS_CODE_SUCCESS != ClusterHelper::FitStart(pCluster, m_nClusterFitLayers, clusterFitResult))
                continue;

            // Compare cluster direction with the projected helix direction
            const float directionCosine(helixDirection.GetDotProduct(clusterFitResult.GetDirection()));

            if ((directionCosine < m_directionCosineCut) && (pCluster->GetMipFraction() < m_clusterMipFractionCut))
                continue;

            // Use position and direction results to identify track/cluster match
            bool isPossibleMatch(false);

            if (directionCosine > m_directionCosineCut1)
            {
                isPossibleMatch = true;
            }
            else if ((directionCosine > m_directionCosineCut2) && (deltaR < m_deltaRCut2))
            {
                isPossibleMatch = true;
            }
            else if ((directionCosine > m_directionCosineCut3) && (deltaR < m_deltaRCut3))
            {
                isPossibleMatch = true;
            }
            else if ((directionCosine > m_directionCosineCut4) && (deltaR < m_deltaRCut4))
            {
                isPossibleMatch = true;
            }

            if (isPossibleMatch)
            {
                const float energyDifference(std::fabs(pCluster->GetHadronicEnergy() - pTrack->GetEnergyAtDca()));

                if ((deltaR < smallestDeltaR) || ((deltaR == smallestDeltaR) && (energyDifference < minEnergyDifference)))
                {
                    smallestDeltaR = deltaR;
                    pBestCluster = pCluster;
                    minEnergyDifference = energyDifference;
                }
            }
        }

        if (NULL != pBestCluster)
        {
            PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, PandoraContentApi::AddTrackClusterAssociation(*this, pTrack, pBestCluster));
        }
    }

    return STATUS_CODE_SUCCESS;
}

//------------------------------------------------------------------------------------------------------------------------------------------

float LoopingTrackAssociationAlgorithm::GetMeanDeltaR(Cluster *const pCluster, const float helixXCentre, const float helixYCentre,
    const float helixRadius) const
{
    float deltaRSum(0.f);
    unsigned int nContributions(0);

    const PseudoLayer endLayer(pCluster->GetInnerPseudoLayer() + m_nClusterDeltaRLayers);
    const OrderedCaloHitList &orderedCaloHitList(pCluster->GetOrderedCaloHitList());

    for (OrderedCaloHitList::const_iterator iter = orderedCaloHitList.begin(), iterEnd = orderedCaloHitList.end(); iter != iterEnd; ++iter)
    {
        if (iter->first > endLayer)
            break;

        for (CaloHitList::const_iterator hitIter = iter->second->begin(), hitIterEnd = iter->second->end(); hitIter != hitIterEnd; ++hitIter)
        {
            const CartesianVector &hitPosition((*hitIter)->GetPositionVector());
            const float hitDeltaX(hitPosition.GetX() - helixXCentre);
            const float hitDeltaY(hitPosition.GetY() - helixYCentre);

            deltaRSum += std::sqrt((hitDeltaX * hitDeltaX) + (hitDeltaY * hitDeltaY));
            nContributions++;
        }
    }

    if (0 == nContributions)
        throw StatusCodeException(STATUS_CODE_FAILURE);

    const float meanDeltaR((deltaRSum / static_cast<float>(nContributions)) - std::fabs(helixRadius));

    return meanDeltaR;
}

//------------------------------------------------------------------------------------------------------------------------------------------

StatusCode LoopingTrackAssociationAlgorithm::ReadSettings(const TiXmlHandle xmlHandle)
{
    m_maxEndCapDeltaZ = 50.f;
    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle,
        "MaxEndCapDeltaZ", m_maxEndCapDeltaZ));

    m_minHitsInCluster = 4;
    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle,
        "MinHitsInCluster", m_minHitsInCluster));

    m_minOccupiedLayersInCluster = 4;
    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle,
        "MinOccupiedLayersInCluster", m_minOccupiedLayersInCluster));

    m_maxClusterInnerLayer = 9;
    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle,
        "MaxClusterInnerLayer", m_maxClusterInnerLayer));

    m_maxAbsoluteTrackClusterChi = 2.f;
    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle,
        "MaxAbsoluteTrackClusterChi", m_maxAbsoluteTrackClusterChi));

    m_maxDeltaR = 50.f;
    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle,
        "MaxDeltaR", m_maxDeltaR));

    m_minDeltaR = -100.f;
    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle,
        "MinDeltaR", m_minDeltaR));

    m_nClusterFitLayers = 10;
    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle,
        "NClusterFitLayers", m_nClusterFitLayers));

    m_nClusterDeltaRLayers = 9;
    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle,
        "NClusterDeltaRLayers", m_nClusterDeltaRLayers));

    m_directionCosineCut = 0.975f;
    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle,
        "DirectionCosineCut", m_directionCosineCut));

    m_clusterMipFractionCut = 0.5f;
    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle,
        "ClusterMipFractionCut", m_clusterMipFractionCut));

    m_directionCosineCut1 = 0.925f;
    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle,
        "DirectionCosineCut1", m_directionCosineCut1));

    m_directionCosineCut2 = 0.85f;
    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle,
        "DirectionCosineCut2", m_directionCosineCut2));

    m_deltaRCut2 = 50.f;
    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle,
        "DeltaRCut2", m_deltaRCut2));

    m_directionCosineCut3 = 0.75f;
    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle,
        "DirectionCosineCut3", m_directionCosineCut3));

    m_deltaRCut3 = 25.f;
    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle,
        "DeltaRCut3", m_deltaRCut3));

    m_directionCosineCut4 = 0.f;
    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle,
        "DirectionCosineCut4", m_directionCosineCut4));

    m_deltaRCut4 = 10.f;
    PANDORA_RETURN_RESULT_IF_AND_IF(STATUS_CODE_SUCCESS, STATUS_CODE_NOT_FOUND, !=, XmlHelper::ReadValue(xmlHandle,
        "DeltaRCut4", m_deltaRCut4));

    return STATUS_CODE_SUCCESS;
}
