/*===================================================================

MSI applications for interactive analysis in MITK (M2aia)

Copyright (c) Jonas Cordes

All rights reserved.

This software is distributed WITHOUT ANY WARRANTY; without
even the implied warranty of MERCHANTABILITY or FITNESS FOR
A PARTICULAR PURPOSE.

See LICENSE.txt for details.

===================================================================*/
#include <future>
#include <iterator>
#include <m2ImzMLParser.h>
#include <m2Timer.h>
#include <math.h>
#include <numeric>
#include <unordered_map>

auto m2::ImzMLParser::findLine(std::ifstream &f, std::string name, std::string start_tag, bool eol)
  -> unsigned long long
{
  auto line = f.tellg();
  std::string str;
  unsigned int i = 0;

  std::string bb;
  bb += start_tag[0];
  while (std::getline(f, str))
  {
    if (eol)
      line = f.tellg();
    auto pos2 = str.find(start_tag[0]);
    if (pos2 != std::string::npos)
    {
      bool match = true;
      for (i = 1; i < start_tag.size() && (pos2 + i) < str.size(); ++i)
      {
        bb += str[pos2 + i];
        if (str[pos2 + i] != start_tag[i])
        {
          match = false;
          break;
        }
      }

      if (match)
      {
        str = str.substr(pos2 + start_tag.size(), name.size());
        if (str.compare(name) == 0)
        {
          return (unsigned long long)(line);
        }
      }
    }

    line = f.tellg(); // point to beginning of the new line
  }
  return -1;
}

void m2::ImzMLParser::ReadImageMetaData(m2::ImzMLSpectrumImage::Pointer data)
{
  std::ifstream f;
  std::vector<std::string> stack, context_stack;
  std::string line, context, tag, name, value, accession;
  tag.reserve(60);
  name.reserve(60);
  value.reserve(60);
  context.reserve(60);
  accession.reserve(60);

  std::unordered_map<std::string, std::function<void(const std::string &)>> accession_map;
  std::unordered_map<std::string, std::function<void(const std::string &)>> context_map;

  for (auto &source : data->GetImzMLSpectrumImageSourceList())
  {
    f.open((source.m_ImzMLDataPath), std::ios_base::binary);

    std::map<std::string, unsigned> precisionDict = {{"32-bit float", sizeof(float)},
                                                     {"64-bit float", sizeof(double)},
                                                     {"32-bit integer", sizeof(int32_t)},
                                                     {"64-bit integer", sizeof(int64_t)}};

    const auto attributValue =
      [](const std::string &line, const std::string &tag, std::string &attribute) -> std::string &
    {
      auto p = line.find(tag);
      attribute.clear();
      if (p != std::string::npos)
      {
        auto s_value = line.find('"', p) + 1;
        auto e_value = line.find('"', s_value);
        while (s_value != e_value)
          attribute.push_back(line[s_value++]);
      }
      return attribute;
    };

    const auto setTagName = [](const std::string &line, std::string &tag) -> void
    {
      auto p = line.find('<');
      tag.clear();
      if (p != std::string::npos)
      {
        ++p;
        auto e = line.find_first_of(" >", p + 1);
        while (p != e)
          tag.push_back(line[p++]);
      }
    };

    const auto evaluateAccession = [](const auto &line, const auto &accession, const auto &dict) -> bool
    {
      auto keyFuncPairIt = dict.find(accession);
      if (keyFuncPairIt != dict.end())
      {
        keyFuncPairIt->second(line); // call the function
        return true;
      }
      return false;
    };

    const auto evaluateContext = [](const auto &line, const auto &tag, const auto &dict, auto & /*context*/) -> void
    {
      auto keyFuncPairIt = dict.find(tag);
      if (keyFuncPairIt != dict.end())
      {
        return keyFuncPairIt->second(line); // call the function
      }
      return;
    };

    const auto ContextValueToStringProperty = [&](const std::string &line)
    {
      attributValue(line, "name", name);
      attributValue(line, "value", value);
      if (!context.empty())
        data->SetPropertyValue("[" + context + "] " + name, value);
      data->SetPropertyValue(name, value);
    };

    const auto ValueToUnsignedIntProperty = [&](const std::string &line)
    {
      attributValue(line, "name", name);
      attributValue(line, "value", value);
      data->SetPropertyValue<unsigned>(name, std::stoul(value));
    };

    const auto ValueToDoubleProperty = [&](const std::string &line)
    {
      attributValue(line, "name", name);
      attributValue(line, "value", value);
      data->SetPropertyValue<double>(name, std::stod(value));
    };

    {
      // ------ Process a referenceableParameter Group
      context_map["referenceableParamGroup"] = [&](const std::string &line)
      {
        std::string refGroupID = "";
        std::string refGroupName = "";
        std::string targetKey = "";
        attributValue(line, "id", refGroupID);
        std::string gLine;
        const auto npos = std::string::npos;
        while (!f.eof())
        {
          std::getline(f, gLine); // read the next line

          if (gLine.find("</") != std::string::npos)
            break;

          if (gLine.find("MS:1000521") != npos || gLine.find("MS:1000523") != npos)
            attributValue(gLine, "name", name);

          if (gLine.find("MS:1000514") != npos)
          {
            attributValue(gLine, "name", refGroupName);
            targetKey = "mzGroupName";
          }

          if (gLine.find("MS:1000515") != npos)
          {
            attributValue(gLine, "name", refGroupName);
            targetKey = "intensityGroupName";
          }

          if (gLine.find("MS:1000127") != npos || gLine.find("MS:1000128") != npos)
          {
            std::string spectype;
            attributValue(gLine, "name", spectype);
            data->SetPropertyValue<std::string>(spectype, "");
          }
        }
        if (!targetKey.empty())
        {
          data->SetPropertyValue<std::string>(targetKey, refGroupName);
          data->SetPropertyValue<std::string>(refGroupName, refGroupID);
          data->SetPropertyValue<unsigned>(refGroupName + " value type (bytes)", precisionDict[name]);
          data->SetPropertyValue<std::string>(refGroupName + " value type", name);
        }

        return "";
      };

      context_map["software"] = [&](const std::string &line)
      {
        attributValue(line, "id", name);
        attributValue(line, "version", value);
        context = name + " " + value;
      };
      context_map["scanSettings"] = [&](const std::string &line) { attributValue(line, "id", context); };

      accession_map["IMS:1000042"] = [&](const std::string &line)
      {
        attributValue(line, "value", value);
        data->SetPropertyValue<unsigned>("max count of pixel x", std::stoul(value));
      };
      accession_map["IMS:1000043"] = [&](const std::string &line)
      {
        attributValue(line, "value", value);
        data->SetPropertyValue<unsigned>("max count of pixel y", std::stoul(value));
      };
      accession_map["IMS:1000044"] = ValueToUnsignedIntProperty; // max dim x
      accession_map["IMS:1000045"] = ValueToUnsignedIntProperty; // max dim y
      accession_map["IMS:1000046"] = ValueToDoubleProperty;      // pixel size x
      accession_map["IMS:1000047"] = ValueToDoubleProperty;      // pixel size y

      accession_map["IMS:X1"] = accession_map["M2:0000001"] =
        accession_map["IMS:1000053"] = [&](const std::string &line) { // origin x
          attributValue(line, "value", value);
          data->SetPropertyValue<double>("origin x", m2::MicroMeterToMilliMeter(std::stol(value)));
        };

      accession_map["IMS:X2"] = accession_map["M2:0000002"] =
        accession_map["IMS:1000054"] = [&](const std::string &line) { // origin y
          attributValue(line, "value", value);
          data->SetPropertyValue<double>("origin y", m2::MicroMeterToMilliMeter(std::stol(value)));
        };

      accession_map["IMS:X3"] = accession_map["M2:0000003"] = [&](const std::string &line) { // origin y
        attributValue(line, "value", value);
        data->SetPropertyValue<double>("origin z", m2::MicroMeterToMilliMeter(std::stol(value)));
      };

      // origin size z
      // scanSettings
      context_map["instrumentConfiguration"] = [&](const std::string &line) { attributValue(line, "id", context); };

      context_map["source"] = [&](const std::string &) { context = "source"; };
      context_map["analyzer"] = [&](const std::string &) { context = "analyzer"; };
      context_map["detector"] = [&](const std::string &) { context = "detector"; };

      context_map["dataProcessing"] = [&](const std::string &line) { attributValue(line, "id", context); };
      context_map["processingMethod"] = [&](const std::string &line)
      {
        attributValue(line, "order", value);
        context = context + "processingMethod (" + value + ")";
      };

      // default values
      data->SetPropertyValue<unsigned>("max count of pixel z", 1);
      data->SetPropertyValue<double>("pixel size x", -1);
      data->SetPropertyValue<double>("pixel size y", -1);
      data->SetPropertyValue<double>("pixel size z", -1);

      data->SetPropertyValue<double>("origin x", 0.0);
      data->SetPropertyValue<double>("origin y", 0.0);
      data->SetPropertyValue<double>("origin z", 0.0);

      // -------- PROCESS FILE --------
      {
        while (!f.eof())
        {
          std::getline(f, line); // read the next line

          // assuming spectra meta data after ImzML meta data.
          if (line.find("run") != std::string::npos)
            break; // indicates begin of spectra meta data

          // Check for end-tag. If true, pop the last context
          // element from the stack and clear the context string.
          if (line.find("</") != std::string::npos)
          {
            context_stack.pop_back();
            context.clear();
          }
          // Check for empty-element tag. Can be cvParam or userParam (e.g. used by ScilsLab).
          else if (line.rfind("/>") != std::string::npos)
          {
            attributValue(line, "accession", accession);
            if (!accession.empty())
            {
              if (!context.empty())
              {
                // Call context specific accession.
                if (evaluateAccession(line, accession + "[" + context + "]", accession_map))
                  continue; // if evaluation succeeded continue loop and go to next line
              }
              // Fallback if context specific accession is not found.
              auto status = evaluateAccession(line, accession, accession_map);
              if (!status) // Default: name + value is added to IMS data property list
                ContextValueToStringProperty(line);
            }
          }

          else
          { // it is probably a start-tag. Could be a comment ...
            // .. but in a clean file no comments should be available.

            setTagName(line, tag);
            context_stack.push_back(tag);
            evaluateContext(line, tag, context_map, context);
          }
        }
      }

      // -------- POST PROCESS META DATA --------
      if (data->GetProperty("pixel size"))
      {
        auto v = data->GetPropertyValue<double>("pixel size");
        data->SetPropertyValue<double>("pixel size x", v);
      }

      if (data->GetPropertyValue<double>("pixel size y") == -1 && data->GetPropertyValue<double>("pixel size x") > 0)
      {
        // Only IMS:1000046 was set (should be standard)
        auto v = data->GetPropertyValue<double>("pixel size x");
        double sqs = std::sqrt(v);
        data->SetPropertyValue<double>("pixel size x", m2::MicroMeterToMilliMeter(sqs));
        data->SetPropertyValue<double>("pixel size y", m2::MicroMeterToMilliMeter(sqs));
        data->SetPropertyValue<double>("squared pixel size", v);
      }
      else if (data->GetPropertyValue<double>("pixel size y") > 0 && data->GetPropertyValue<double>("pixel size x") > 0)
      {
        // IMS:1000046 and IMS:1000047 was set
        // Note IMS:1000047 is used in a false way
        // -> in this case assumed to take the y value
        // if pixel size y is used > do not sqrt
        auto pixelSizeXInMilliMetre = m2::MicroMeterToMilliMeter(data->GetPropertyValue<double>("pixel size x"));
        auto pixelSizeYInMilliMetre = m2::MicroMeterToMilliMeter(data->GetPropertyValue<double>("pixel size y"));
        data->SetPropertyValue("pixel size x", pixelSizeXInMilliMetre);
        data->SetPropertyValue("pixel size y", pixelSizeYInMilliMetre);
      }

      if (data->GetPropertyValue<double>("pixel size y") <= 0 && data->GetPropertyValue<double>("pixel size x") <= 0)
      {
        data->SetPropertyValue("pixel size x", m2::MicroMeterToMilliMeter(50));
        data->SetPropertyValue("pixel size y", m2::MicroMeterToMilliMeter(50));
        data->SetPropertyValue(
          "pixel size info",
          std::string("Pixel size x and y are default values, due to missing imzTags IMS:1000046 and IMS:1000047!"));
        MITK_WARN << "No pixel size found, set x and y spacing to 50 microns!";
      }

      if (data->GetPropertyValue<double>("pixel size z") < 0)
      {
        data->SetPropertyValue("pixel size z", m2::MicroMeterToMilliMeter(10));
        data->SetPropertyValue<unsigned>("max count of pixel z", 1);
      }
      else
      {
        auto pixelSizeZInMilliMetre = m2::MicroMeterToMilliMeter(data->GetPropertyValue<double>("pixel size z"));
        data->SetPropertyValue("pixel size z", pixelSizeZInMilliMetre);
      }
      // -------- END META DATA --------
    }

    //{
    //	accession_map.clear();
    //	context_map.clear();

    //	auto & spectra = data->Spectra();

    //	context_map["spectrumList"] = [&](auto line) {
    //		unsigned count = std::stoul(attributValue(line, "count", value));
    //		data->SetPropertyValue<unsigned>("number of measurments", count);
    //		spectra.resize(count);
    //	};

    //	unsigned long long spectrumIndexReference;

    //	context_map["spectrum"] = [&](auto line) {
    //		spectrumIndexReference = std::stoull(attributValue(line, "index", value));
    //		spectra[spectrumIndexReference].index.z = 1;
    //	};

    //	accession_map["IMS:1000050"] = [&](auto line) {
    //		spectra[spectrumIndexReference].index.x = std::stoul(attributValue(line, "value", value)); };
    //	accession_map["IMS:1000051"] = [&](auto line) {
    //		spectra[spectrumIndexReference].index.y = std::stoul(attributValue(line, "value", value)); };
    //	accession_map["IMS:1000052"] = [&](auto line) {
    //		spectra[spectrumIndexReference].index.z = std::stoul(attributValue(line, "value", value)); };
    //	accession_map["3DPositionX"] = [&](auto line) {
    //		spectra[spectrumIndexReference].world.x = std::stod(attributValue(line, "value", value)); };
    //	accession_map["3DPositionY"] = [&](auto line) {
    //		spectra[spectrumIndexReference].world.y = std::stod(attributValue(line, "value", value)); };
    //	accession_map["3DPositionZ"] = [&](auto line) {
    //		spectra[spectrumIndexReference].world.z = std::stod(attributValue(line, "value", value)); };
    //
    //
    //	accession_map["MS:1000285"] = [&](auto line) {
    //		spectra[spectrumIndexReference].normalize = std::stod(attributValue(line, "value", value)); };

    //	context_map["referenceableParamGroupRef"] = [&](auto line) { attributValue(line, "ref", context); };

    //	auto mzArrayRefName = data->GetPropertyValue<std::string>("m/z array");
    //	accession_map["IMS:1000103[" + mzArrayRefName + "]"] = [&](auto line) {spectra[spectrumIndexReference].mzLength
    //=
    // std::stoull(attributValue(line, "value", value)); }; 	accession_map["IMS:1000102[" + mzArrayRefName + "]"] =
    // [&](auto line) {spectra[spectrumIndexReference].mzOffset = std::stoull(attributValue(line, "value", value)); };

    //	auto intensityArrayRefName = data->GetPropertyValue<std::string>("intensity array");
    //	accession_map["IMS:1000103[" + intensityArrayRefName + "]"] = [&](auto line)
    //{spectra[spectrumIndexReference].intLength = std::stoull(attributValue(line, "value", value)); };
    //	accession_map["IMS:1000102[" + intensityArrayRefName + "]"] = [&](auto line)
    //{spectra[spectrumIndexReference].intOffset = std::stoull(attributValue(line, "value", value)); };

    //	std::vector<char> buff;
    //	std::list<std::thread> threads;

    //	{
    //		while (!f.eof()) {
    //			std::getline(f, line); // read the next line from the file
    //			if (line.find("</") != std::string::npos) continue; // check for end-tag
    //			if (line.rfind("/>") != std::string::npos) { // check for empty element-tag
    //				attributValue(line, "accession", accession);
    //				if (!accession.empty()) {
    //					// if a context is given
    //					if (!context.empty()) {
    //						// call context specific accession
    //						if (evaluateAccession(line, accession + "[" + context + "]", accession_map)) continue;

    //					}
    //					// fall back to context-less accession
    //					if (evaluateAccession(line, accession, accession_map))	continue;

    //				}
    //				else {
    //					// e.g. support old 3D imzML Data (SciLs specific tags)
    //					attributValue(line, "name", name);
    //					if (name.compare("3DPositionZ") == 0)
    //						evaluateAccession(line, name, accession_map);
    //				}
    //			}

    //			setTagName(line, tag);
    //			evaluateContext(line, tag, context_map, context);

    //		}

    //	}

    //	std::set<unsigned int> uniques;
    //	for (auto & s : spectra)
    //		uniques.insert(s.world.z);

    //	if (uniques.size() > 1) {
    //		unsigned zSpacing; // is one 1mm, if sliced date mitk can go better with this z spacing
    //		unsigned zCount;
    //		std::list<double> diffs;

    //		auto a = uniques.begin();
    //		auto b = std::next(uniques.begin(), 1);
    //		zSpacing = (*b) - (*a);
    //		zCount = uniques.size();
    //		bool startsByZero = uniques.find(0) != uniques.end();

    //		// Transform z physical coordinates to index coordinates
    //		for (auto & s : spectra)
    //			s.index.z = startsByZero ? (s.world.z + zSpacing) / zSpacing : s.world.z / zSpacing;

    //		data->SetPropertyValue<unsigned>("max count of pixel z", zCount);
    //		data->SetPropertyValue<double>("pixel size z", zSpacing *1e-3);

    //	}
    //}
  }
}

void m2::ImzMLParser::ReadImageSpectrumMetaData(m2::ImzMLSpectrumImage::Pointer data)
{
  std::ifstream f;
  std::vector<std::string> stack, context_stack;
  std::string line, context, tag, name, value, accession;
  tag.reserve(60);
  name.reserve(60);
  value.reserve(60);
  context.reserve(60);
  accession.reserve(60);

  std::unordered_map<std::string, std::function<void(const std::string &)>> accession_map;
  std::unordered_map<std::string, std::function<void(const std::string &)>> context_map;
  for (auto &source : data->GetImzMLSpectrumImageSourceList())
  {
    f.open((source.m_ImzMLDataPath), std::ios_base::binary);

    std::map<std::string, unsigned> precisionDict = {{"32-bit float", sizeof(float)},
                                                     {"64-bit float", sizeof(double)},
                                                     {"32-bit integer", sizeof(int32_t)},
                                                     {"64-bit integer", sizeof(int64_t)}};

    const auto attributValue =
      [](const std::string &line, const std::string &tag, std::string &attribute) -> std::string &
    {
      auto p = line.find(tag);
      attribute.clear();
      if (p != std::string::npos)
      {
        auto s_value = line.find('"', p) + 1;
        auto e_value = line.find('"', s_value);
        while (s_value != e_value)
          attribute.push_back(line[s_value++]);
      }
      return attribute;
    };

    const auto setTagName = [](const std::string &line, std::string &tag) -> void
    {
      auto p = line.find('<');
      tag.clear();
      if (p != std::string::npos)
      {
        ++p;
        auto e = line.find_first_of(" >", p + 1);
        while (p != e)
          tag.push_back(line[p++]);
      }
    };

    const auto evaluateAccession = [](const auto &line, const auto &accession, const auto &dict) -> bool
    {
      auto keyFuncPairIt = dict.find(accession);
      if (keyFuncPairIt != dict.end())
      {
        keyFuncPairIt->second(line); // call the function
        return true;
      }
      return false;
    };

    const auto evaluateContext = [](const auto &line, const auto &tag, const auto &dict, auto & /*context*/) -> void
    {
      auto keyFuncPairIt = dict.find(tag);
      if (keyFuncPairIt != dict.end())
      {
        return keyFuncPairIt->second(line); // call the function
      }
      return;
    };

    {
      accession_map.clear();
      context_map.clear();

      auto &spectra = source.m_Spectra;

      context_map["spectrumList"] = [&](auto line)
      {
        unsigned count = std::stoul(attributValue(line, "count", value));
        data->SetPropertyValue<unsigned>("number of measurements", count);
        spectra.resize(count);
      };

      size_t spectrumIndexReference = 0;

      context_map["spectrum"] = [&](auto line)
      {
        attributValue(line, "index", value);
        spectra[spectrumIndexReference].index.SetElement(2, 0);
        // TODO: Backtracability to spectrum in file may be lost.
        //  Maybe we can add the id string to the BinarySpectrumMetaData definition.
        //  e.g. spectrum.tag = attributValue(line, "id", value);
      };

      accession_map["IMS:1000050"] = [&](auto line)
      { spectra[spectrumIndexReference].index.SetElement(0, std::stoul(attributValue(line, "value", value)) - 1); };
      accession_map["IMS:1000051"] = [&](auto line)
      { spectra[spectrumIndexReference].index.SetElement(1, std::stoul(attributValue(line, "value", value)) - 1); };
      accession_map["IMS:1000052"] = [&](auto line)
      { spectra[spectrumIndexReference].index.SetElement(2, std::stoul(attributValue(line, "value", value)) - 1); };
      accession_map["3DPositionX"] = [&](auto line)
      { spectra[spectrumIndexReference].world.x = std::stod(attributValue(line, "value", value)); };
      accession_map["3DPositionY"] = [&](auto line)
      { spectra[spectrumIndexReference].world.y = std::stod(attributValue(line, "value", value)); };
      accession_map["3DPositionZ"] = [&](auto line)
      { spectra[spectrumIndexReference].world.z = std::stod(attributValue(line, "value", value)); };

      accession_map["MS:1000285"] = [&](auto line)
      { spectra[spectrumIndexReference].inFileNormalizationFactor = std::stod(attributValue(line, "value", value)); };

      context_map["referenceableParamGroupRef"] = [&](auto line) { attributValue(line, "ref", context); };

      auto mzArrayRefName = data->GetPropertyValue<std::string>("m/z array");
      accession_map["IMS:1000103[" + mzArrayRefName + "]"] = [&](auto line)
      { spectra[spectrumIndexReference].mzLength = std::stoull(attributValue(line, "value", value)); };
      accession_map["IMS:1000102[" + mzArrayRefName + "]"] = [&](auto line)
      { spectra[spectrumIndexReference].mzOffset = std::stoull(attributValue(line, "value", value)); };

      auto intensityArrayRefName = data->GetPropertyValue<std::string>("intensity array");
      accession_map["IMS:1000103[" + intensityArrayRefName + "]"] = [&](auto line)
      { spectra[spectrumIndexReference].intLength = std::stoull(attributValue(line, "value", value)); };
      accession_map["IMS:1000102[" + intensityArrayRefName + "]"] = [&](auto line)
      { spectra[spectrumIndexReference].intOffset = std::stoull(attributValue(line, "value", value)); };

      std::vector<char> buff;
      std::list<std::thread> threads;
      bool _ScilsTag3DCoordinateUsed = false;
      {
        while (!f.eof())
        {
          std::getline(f, line); // read the next line from the file
          if (line.find("</") != std::string::npos)
          {
            if (line.find("spectrum") != std::string::npos)
              ++spectrumIndexReference;
            continue; // check for end-tag
          }
          if (line.rfind("/>") != std::string::npos)
          { // check for empty element-tag
            attributValue(line, "accession", accession);
            if (!accession.empty())
            {
              // if a context is given
              if (!context.empty())
              {
                // call context specific accession
                if (evaluateAccession(line, accession + "[" + context + "]", accession_map))
                  continue;
              }
              // fall back to context-less accession
              if (evaluateAccession(line, accession, accession_map))
                continue;
            }
            else
            {
              // e.g. support old 3D imzML Data (SciLs specific tags)
              attributValue(line, "name", name);
              if (name.compare("3DPositionZ") == 0)
              {
                evaluateAccession(line, name, accession_map);
                _ScilsTag3DCoordinateUsed = true;
              }
            }
          }

          setTagName(line, tag);
          evaluateContext(line, tag, context_map, context);
        }
      }

      std::set<unsigned int> uniques;
      if (_ScilsTag3DCoordinateUsed)
      { // check z world uniques
        // MITK_INFO << "SciLs 3D tag found";
        for (auto &s : spectra)
          uniques.insert(s.world.z);

        // MITK_INFO << "\t" << uniques.size() << " unique z positions found:";
        // std::copy(std::begin(uniques), std::end(uniques), std::ostream_iterator<unsigned int>{std::cout, ", "});

        std::map<unsigned, unsigned> worldToIndexMap;
        unsigned i = 0;
        for (const auto &u : uniques)
          worldToIndexMap[u] = i++;

        if (uniques.size() > 1)
        {
          unsigned zSpacing;
          unsigned zCount;
          std::list<unsigned> diffs, diffs_uniques;
          std::adjacent_difference(std::begin(uniques), std::end(uniques), std::back_inserter(diffs));
          diffs_uniques = diffs;
          diffs_uniques.sort();
          diffs_uniques.erase(std::unique(std::begin(diffs_uniques), std::end(diffs_uniques)), std::end(diffs_uniques));
          // MITK_INFO << "\t" << diffs_uniques.size() << " unique z distances found:";
          // std::copy(
          //   std::begin(diffs_uniques), std::end(diffs_uniques), std::ostream_iterator<unsigned>{std::cout, ", "});
          unsigned maxCount = 0;
          for (auto &uDiff : diffs_uniques)
          {
            unsigned count = std::count(std::begin(diffs), std::end(diffs), uDiff);
            // MITK_INFO << "Different values " << uDiff << " were found " << count << " times.";
            if (maxCount < count)
            {
              maxCount = count;
              zSpacing = uDiff;
            }
          }

          auto a = uniques.begin();
          auto b = std::next(uniques.begin(), 1);
          zSpacing = (*b) - (*a);
          zCount = uniques.size();
          //          bool startsByZero = uniques.find(0) != uniques.end();

          // Transform z physical coordinates to index coordinates
          for (auto &s : spectra)
            s.index.SetElement(2, worldToIndexMap[static_cast<unsigned>(s.world.z)]);

          data->SetPropertyValue<unsigned>("max count of pixel z", zCount);
          data->SetPropertyValue<double>("pixel size z", m2::MicroMeterToMilliMeter(zSpacing));
        }
      }
      else
      { // check index z uniques
        uniques.clear();
        for (auto &s : spectra)
          uniques.insert(s.index[2]);

        if (uniques.size() > 1)
        {
          data->SetPropertyValue<unsigned>("max count of pixel z", uniques.size());
          // is set to 10 micrometers fix (can be changed in app)
          data->SetPropertyValue<double>("pixel size z", m2::MicroMeterToMilliMeter(10));
        }
      }
    }
  }
}

// std::string m2::ImzMLXMLParser::WriteMetaData(m2::ImzMLSpectrumImage::Pointer /*val*/, std::string &path)
//{
//  const auto &meta = mitk::ImzMLXMLTemplate::IMZML_TEMPLATE_START;
//  const auto &spectrum = mitk::ImzMLXMLTemplate::IMZML_SPECTRUM_TEMPLATE;
//
//  std::string mode = "continuous", polarity = "positive scan", mz_data_type = "16-bit float",
//              mz_compression = "no compression", int_data_type = "32-bit float", int_compression = "no compression",
//              size_x = "20", size_y = "20", run_id = "Hello ImzML", num_spectra = "400";
//
//  std::ofstream f(path, std::ofstream::binary);
//
//  std::map<std::string, std::string> meta_context{
//    {"mode", mode},
//    {"polarity", polarity},
//    {"mz_data_type", mz_data_type},
//    {"mz_compression", mz_compression},
//    {"int_data_type", int_data_type},
//    {"int_compression", int_compression},
//    {"size_x", size_x},
//    {"size_y", size_y},
//    {"run_id", run_id},
//    //{"uuid", suuid},
//    {"num_spectra", num_spectra},
//    {"mode_code", mitk::ImzMLXMLTemplate::obo_codes.at(mode)},
//    {"polarity_code", mitk::ImzMLXMLTemplate::obo_codes.at(polarity)},
//    {"mz_data_type_code", mitk::ImzMLXMLTemplate::obo_codes.at(mz_data_type)},
//    {"int_data_type_code", mitk::ImzMLXMLTemplate::obo_codes.at(int_data_type)}};
//
//  // std::cout << mstch::render(meta, meta_context) << std::endl;
//  f << mitk::ImzMLXMLTemplate::render(meta, meta_context);
//
//  // index
//  // x,y,z
//  // use_z
//  unsigned long long mz_len = 0, mz_offset = 0;
//  unsigned long long int_len = 0, int_offset = 0;
//  // use_encoded_length
//  for (int i = 0; i < 400; ++i)
//  {
//    std::map<std::string, std::string> spectrum_context{{"index", std::to_string(i)},
//                                                        {"x", std::to_string(0)},
//                                                        {"y", std::to_string(0)},
//                                                        {"z", std::to_string(0)},
//                                                        {"mz_len", std::to_string(mz_len)},
//                                                        {"mz_offset", std::to_string(mz_offset)},
//                                                        {"int_len", std::to_string(int_len)},
//                                                        {"int_offset", std::to_string(int_offset)}};
//
//    f << mitk::ImzMLXMLTemplate::render(spectrum, spectrum_context);
//  }
//
//  return "";
//  // f << mitk::ImzMLXMLTemplate::IMZML_TEMPLATE_END;
//}
