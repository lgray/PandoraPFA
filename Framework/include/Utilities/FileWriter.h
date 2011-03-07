/**
 *  @file   PandoraPFANew/Framework/include/Utilities/FileWriter.h
 * 
 *  @brief  Header file for the file writer class.
 * 
 *  $Log: $
 */
#ifndef FILE_WRITER_H
#define FILE_WRITER_H 1

#include "Helpers/GeometryHelper.h"

#include "Pandora/Pandora.h"
#include "Pandora/PandoraIO.h"
#include "Pandora/StatusCodes.h"

#include "Objects/CartesianVector.h"
#include "Objects/TrackState.h"

#include <fstream>
#include <string>

namespace pandora
{

/**
 *  @brief  FileWriter class
 */
class FileWriter
{
public:
    /**
     *  @brief  Constructor
     * 
     *  @param  algorithm the pandora instance to be used alongside the file writer
     *  @param  fileName the name of the output file
     *  @param  fileMode the mode for file writing
     */
    FileWriter(const pandora::Pandora &pandora, const std::string &fileName, const FileMode fileMode = APPEND);

    /**
     *  @brief  Destructor
     */
    ~FileWriter();

    /**
     *  @brief  Write the current geometry information to the file
     */
    StatusCode WriteGeometry();

    /**
     *  @brief  Write the current event to the file
     */
    StatusCode WriteEvent();

    /**
     *  @brief  Write the specified event components to the file
     * 
     *  @param  trackList the list of tracks to write to the file
     *  @param  orderedCaloHitList the ordered list of calo hits to write to the file
     */
    StatusCode WriteEvent(const TrackList &trackList, const OrderedCaloHitList &orderedCaloHitList);

private:
   /**
     *  @brief  Write the container header to the file
     * 
     *  @param  containerId the container id
     */
    StatusCode WriteHeader(const ContainerId containerId);

    /**
     *  @brief  Write the container footer to the file
     */
    StatusCode WriteFooter();

    /**
     *  @brief  Write the geometry parameters to the file
     */
    StatusCode WriteGeometryParameters();

    /**
     *  @brief  Write a sub detector to the current position in the file
     * 
     *  @param  pSubDetectorParameters address of the sub detector parameters
     */
    StatusCode WriteSubDetector(const GeometryHelper::SubDetectorParameters *const pSubDetectorParameters);

    /**
     *  @brief  Write a calo hit to the current position in the file
     * 
     *  @param  pCaloHit address of the calo hit
     */
    StatusCode WriteCaloHit(const CaloHit *const pCaloHit);

    /**
     *  @brief  Write a track to the current position in the file
     * 
     *  @param   pTrack address of the track
     */
    StatusCode WriteTrack(const Track *const pTrack);

    /**
     *  @brief  Write a track list to the current position in the file
     * 
     *  @param  trackList the track list
     */
    StatusCode WriteTrackList(const TrackList &trackList);

    /**
     *  @brief  Write an ordered calo hit list to the current position in the file
     * 
     *  @param  orderedCaloHitList the ordered calo hit list
     */
    StatusCode WriteOrderedCaloHitList(const OrderedCaloHitList &orderedCaloHitList);

    /**
     *  @brief  Read a variable from the file
     */
    template<typename T>
    StatusCode WriteVariable(const T &t);

    const pandora::Pandora *const   m_pPandora;             ///< Address of pandora instance to be used alongside the file writer
    ContainerId                     m_containerId;          ///< The type of container currently being written to file
    std::ofstream::pos_type         m_containerPosition;    ///< Position of start of the current event/geometry container object in file
    std::ofstream                   m_fileStream;           ///< The stream class to write to the file
};

//------------------------------------------------------------------------------------------------------------------------------------------

template<typename T>
inline StatusCode FileWriter::WriteVariable(const T &t)
{
    m_fileStream.write(reinterpret_cast<const char*>(&t), sizeof(T));

    if (!m_fileStream.good())
        return STATUS_CODE_FAILURE;

    return STATUS_CODE_SUCCESS;
}

template<>
inline StatusCode FileWriter::WriteVariable(const std::string &t)
{
    // Not currently supported
    return STATUS_CODE_INVALID_PARAMETER;
}

template<>
inline StatusCode FileWriter::WriteVariable(const CartesianVector &t)
{
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteVariable(t.GetX()));
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteVariable(t.GetY()));
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteVariable(t.GetZ()));
    return STATUS_CODE_SUCCESS;
}

template<>
inline StatusCode FileWriter::WriteVariable(const TrackState &t)
{
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteVariable(t.GetPosition()));
    PANDORA_RETURN_RESULT_IF(STATUS_CODE_SUCCESS, !=, this->WriteVariable(t.GetMomentum()));
    return STATUS_CODE_SUCCESS;
}

} // namespace pandora

#endif // #ifndef FILE_WRITER_H