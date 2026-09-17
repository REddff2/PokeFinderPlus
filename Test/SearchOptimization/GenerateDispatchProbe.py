from pathlib import Path
import sys
source=Path(sys.argv[1]).read_text(encoding="utf-8")
source=source.replace('#include "Wild5.hpp"','#include <Form/Gen5/Wild5.hpp>\n#include "IVRangeProbe.hpp"')
start=source.index("    SearcherBase5<WildGenerator5, WildState5> *searcher;")
source=source[:start]+'''    IVRangeProbe::row = {{"fastSearchEnabled",fastSearchEnabled()}, {"iv_cache_present",ivCache!=nullptr},
        {"payload_guard",generator.payloadFirstEnabled(initialIVAdvances,maxIVAdvances)},
        {"reduced_mt_guard",generator.reducedIVsEnabled(initialIVAdvances,maxIVAdvances)},
        {"cache_status",ui->labelIVFastSearch->text().toStdString()}};
'''+source[start:]
for name in ["WildSearcher5CacheFast","WildSearcher5Fast","WildSearcher5"]:
    source=source.replace("searcher = new "+name+"(", 'IVRangeProbe::row["searcher_class"]="'+name+'";\n            searcher = new '+name+'(')
marker="    searcher->setMaxProgress(searcher->getMaxProgress(start, end));"
assert source.count(marker)==1
source=source.replace(marker,'''    auto *ordinary = dynamic_cast<WildSearcher5*>(searcher);
    IVRangeProbe::row["gpu_session"] = ordinary && ordinary->getGpuSession()!=nullptr;
    IVRangeProbe::row["actual_payload_first"] = ordinary && generator.payloadFirstEnabled(initialIVAdvances,maxIVAdvances);
    IVRangeProbe::row["actual_reduced_mt"] = ordinary && generator.reducedIVsEnabled(initialIVAdvances,maxIVAdvances);
    IVRangeProbe::row["workers_started"] = 0;
    delete searcher;
    ui->pushButtonSearch->setEnabled(true);ui->pushButtonCancel->setEnabled(false);
    return;
'''+marker)
Path(sys.argv[2]).write_text(source,encoding="utf-8")
