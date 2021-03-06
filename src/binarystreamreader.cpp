/*
  Copyright © 2018 Hasan Yavuz Özderya

  This file is part of serialplot.

  serialplot is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  serialplot is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with serialplot.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <QtEndian>
#include <QtDebug>

#include "binarystreamreader.h"
#include "floatswap.h"

BinaryStreamReader::BinaryStreamReader(QIODevice* device, ChannelManager* channelMan,
                                       DataRecorder* recorder, QObject* parent) :
    AbstractReader(device, channelMan, recorder, parent)
{
    paused = false;
    skipByteRequested = false;
    skipSampleRequested = false;

    _numOfChannels = _settingsWidget.numOfChannels();
    connect(&_settingsWidget, &BinaryStreamReaderSettings::numOfChannelsChanged,
            this, &BinaryStreamReader::numOfChannelsChanged);
    connect(&_settingsWidget, &BinaryStreamReaderSettings::numOfChannelsChanged,
            this, &BinaryStreamReader::onNumOfChannelsChanged);

    // initial number format selection
    onNumberFormatChanged(_settingsWidget.numberFormat());
    connect(&_settingsWidget, &BinaryStreamReaderSettings::numberFormatChanged,
            this, &BinaryStreamReader::onNumberFormatChanged);

    // enable skip byte and sample buttons
    connect(&_settingsWidget, &BinaryStreamReaderSettings::skipByteRequested,
            [this]()
            {
                skipByteRequested = true;
            });
    connect(&_settingsWidget, &BinaryStreamReaderSettings::skipSampleRequested,
            [this]()
            {
                skipSampleRequested = true;
            });
}

QWidget* BinaryStreamReader::settingsWidget()
{
    return &_settingsWidget;
}

unsigned BinaryStreamReader::numOfChannels()
{
    return _numOfChannels;
}

void BinaryStreamReader::enable(bool enabled)
{
    if (enabled)
    {
        QObject::connect(_device, &QIODevice::readyRead,
                         this, &BinaryStreamReader::onDataReady);
    }
    else
    {
        QObject::disconnect(_device, 0, this, 0);
    }
}

void BinaryStreamReader::pause(bool enabled)
{
    paused = enabled;
}

void BinaryStreamReader::onNumberFormatChanged(NumberFormat numberFormat)
{
    switch(numberFormat)
    {
        case NumberFormat_uint8:
            sampleSize = 1;
            readSample = &BinaryStreamReader::readSampleAs<quint8>;
            break;
        case NumberFormat_int8:
            sampleSize = 1;
            readSample = &BinaryStreamReader::readSampleAs<qint8>;
            break;
        case NumberFormat_uint16:
            sampleSize = 2;
            readSample = &BinaryStreamReader::readSampleAs<quint16>;
            break;
        case NumberFormat_int16:
            sampleSize = 2;
            readSample = &BinaryStreamReader::readSampleAs<qint16>;
            break;
        case NumberFormat_uint32:
            sampleSize = 4;
            readSample = &BinaryStreamReader::readSampleAs<quint32>;
            break;
        case NumberFormat_int32:
            sampleSize = 4;
            readSample = &BinaryStreamReader::readSampleAs<qint32>;
            break;
        case NumberFormat_float:
            sampleSize = 4;
            readSample = &BinaryStreamReader::readSampleAs<float>;
            break;
        case NumberFormat_INVALID:
            Q_ASSERT(1); // never
            break;
    }
}

void BinaryStreamReader::onNumOfChannelsChanged(unsigned value)
{
    _numOfChannels = value;
}

void BinaryStreamReader::onDataReady()
{
    // a package is a set of channel data like {CHAN0_SAMPLE, CHAN1_SAMPLE...}
    int packageSize = sampleSize * _numOfChannels;
    int bytesAvailable = _device->bytesAvailable();

    // skip 1 byte if requested
    if (skipByteRequested && bytesAvailable > 0)
    {
        _device->read(1);
        skipByteRequested = false;
        bytesAvailable--;
    }

    // skip 1 sample (channel) if requested
    if (skipSampleRequested && bytesAvailable >= (int) sampleSize)
    {
        _device->read(sampleSize);
        skipSampleRequested = false;
        bytesAvailable -= sampleSize;
    }

    if (bytesAvailable < packageSize) return;

    int numOfPackagesToRead =
        (bytesAvailable - (bytesAvailable % packageSize)) / packageSize;

    if (paused)
    {
        // read and discard data
        _device->read(numOfPackagesToRead*packageSize);
        return;
    }

    double* channelSamples = new double[numOfPackagesToRead*_numOfChannels];

    for (int i = 0; i < numOfPackagesToRead; i++)
    {
        for (unsigned int ci = 0; ci < _numOfChannels; ci++)
        {
            // channelSamples[ci].replace(i, (this->*readSample)());
            channelSamples[ci*numOfPackagesToRead+i] = (this->*readSample)();
        }
    }

    addData(channelSamples, numOfPackagesToRead*_numOfChannels);

    delete[] channelSamples;
}

template<typename T> double BinaryStreamReader::readSampleAs()
{
    T data;

    _device->read((char*) &data, sizeof(data));

    if (_settingsWidget.endianness() == LittleEndian)
    {
        data = qFromLittleEndian(data);
    }
    else
    {
        data = qFromBigEndian(data);
    }

    return double(data);
}

void BinaryStreamReader::saveSettings(QSettings* settings)
{
    _settingsWidget.saveSettings(settings);
}

void BinaryStreamReader::loadSettings(QSettings* settings)
{
    _settingsWidget.loadSettings(settings);
}
