#include <string>
#include <deque>

#include <iostream>
#include <iomanip>

using std::cin;
using std::cout;
using std::cerr;
using std::endl;

#include "ecgsqa_algos.h"

using namespace ecgsqa;

#include "3p_wfdb/wfdb.h"
#include "3p_wfdb/ecgcodes.h"
#include "ecgsqa_utils2.h"

int wfdb_export(int argc, char **argv) {
                try {
                    _cfg_init(bsToWs(_argv2pltree((const char **) argv, 1, argc)));

                    string name_src_wfdb = +cfg / "name_src_wfdb" / "";
                    name_src_wfdb ="slp04";
                    unity channels = +cfg / "channels" / unity();
                    channels.rx<utStringArray>();
                    channels.arrlb_(0);
                    unity fnp_src_tdb =
                            +cfg / "fnp_src_tdb" / unity(); // |file1|file2... or single filename (string)

                    long t_start = +cfg / "t_start" / -1;
                    if (t_start < 0) { t_start = 0; } // in samples
                    long t_end = +cfg / "t_end" / -1; // in samples

                    string fnp_dest = +cfg / "fnp_dest" / "";
                    fnp_dest = "slp04_khs.txt";
                    bool b_append = +cfg / "append" / false; // false will delete previous file fnp_dest
                    b_append = false;
                    bool b_save_beats = +cfg / "save_beats" / false;
                    b_save_beats = true;
                    bool b_save_beats_eval = +cfg / "save_beats_eval" / false;
                    bool b_save_slpst = +cfg / "save_slpst" / false;
                    b_save_slpst = true;
                    int sec = 0;
                    if (b_save_beats) { sec |= ecg_data::dba_beats; }
                    if (b_save_beats_eval) { sec |= ecg_data::dba_beats_eval; }
                    if (b_save_slpst) { sec |= ecg_data::dba_slpst; }

                    int res = 0;

                    while (1) // once
                    {
                        if (fnp_dest.empty()) {
                            cerr << "ERR [1]: fnp_dest must be non-empty." << endl;
                            res = 10;
                            break;
                        }
                        if (channels.arrsz() & 1) {
                            cerr
                                    << "ERR [2]: channels must be array of pairs |i_src1|dest_name1|i_src2|dest_name2..."
                                    << endl;
                            res = 10;
                            break;
                        }
                        if (!(fnp_src_tdb.isEmpty() || fnp_src_tdb.isString() || fnp_src_tdb.isArray())) {
                            cerr << "ERR [3]: Wrong fnp_src, must be filename or array |file1|file2..." << endl;
                            res = 10;
                            break;
                        }
                        break;
                    }

                    if (res != 0) { return res; }

                    double fd = 250;
                    if (!name_src_wfdb.empty())
                        while (1) // once
                        {
                            long nsig = isigopen(&name_src_wfdb[0], 0, 0);
                            if (nsig <= 0) {
                                res = 11;
                                break;
                            }
                            fd = getifreq();
                            basic_string<WFDB_siginfo> sigs;
                            sigs.resize(nsig);
                            basic_string<WFDB_Sample> data(nsig, 0);

                            ecg_data dat(fd);

                            vector<int> rdlist;
                vector<basic_string<ecgfvsmp> *> rdptr;
                int ichnmax = -1;
                for (s_long jf = 0; jf < channels.arrsz() / 2; ++jf) {
                    int ichn = channels.vint(jf * 2);
                    if (ichn > ichnmax) { ichnmax = ichn; }
                    string name = channels.vcstr(jf * 2 + 1);
                    if (name == "ECG") {
                        sec |= ecg_data::db_ECG;
                        rdlist.push_back(ichn);
                        rdptr.push_back(&dat.ecg);
                    }
                    else if (name == "Resp") {
                        sec |= ecg_data::db_Resp;
                        rdlist.push_back(ichn);
                        rdptr.push_back(&dat.resp);
                    }
                    else if (name == "EEG") {
                        sec |= ecg_data::db_EEG;
                        rdlist.push_back(ichn);
                        rdptr.push_back(&dat.eeg);
                    }
                    else if (name == "BP") {
                        sec |= ecg_data::db_BP;
                        rdlist.push_back(ichn);
                        rdptr.push_back(&dat.bp);
                    }
                }

                if (ichnmax + 1 > nsig) {
                    res = 17;
                    break;
                }

                res = isigopen(&name_src_wfdb[0], &sigs[0], nsig);
                if (res <= 0) {
                    res = 12;
                    break;
                }
                long nsmp = sigs[0].nsamp;
                if (nsmp < 1) {
                    res = 13;
                    break;
                }
                if (t_end < 0) { t_end = nsmp; } else { if (t_end > nsmp) { t_end = nsmp; }}

                vec2_t<ecgfvsmp> gain_ecg(nsig, ecgfvsmp(WFDB_DEFGAIN));
                for (size_t j = 0; j < rdlist.size(); ++j) {
                    int ichn = rdlist[j];
                    if (ichn >= 0 && ichn < nsig) {
                        WFDB_siginfo &sig = sigs[ichn];
                        double x = ecgfvsmp(sig.gain);
                        if (!x) { x = ecgfvsmp(WFDB_DEFGAIN); }
                        gain_ecg[ichn] = x;
                    }
                }

                for (long i = 0; i < nsmp; ++i) {
                    res = getvec(&data[0]);
                    if (res <= 0) {
                        res = 14;
                        break;
                    }
                    if (!(i >= t_start && i < t_end)) { continue; }
                    for (size_t j = 0; j < rdlist.size(); ++j) {
                        int ichn = rdlist[j];
                        rdptr[j]->push_back((data[ichn] - sigs[ichn].baseline) / gain_ecg[ichn]);
                    }
                }


                if (b_save_slpst) {
                    int res2;
                    string annot_class = "st";
                    enum {
                        nanns_max = 1
                    };
                    WFDB_Anninfo anns[nanns_max];
                    anns[0].name = &annot_class[0];
                    anns[0].stat = WFDB_READ;
                    res2 = annopen(&name_src_wfdb[0], &anns[0], nanns_max);
                    if (res2 != 0) {
                        res = 15;
                        break;
                    }

                    WFDB_Annotation z;
                    hashx<string, int> keys;
                    keys["1"] = 1;
                    keys["2"] = 2;
                    keys["3"] = 3;
                    keys["4"] = 4;
                    keys["MT"] = -3;
                    keys["W"] = -1;
                    keys["R"] = 0;
                    int ph_prev = -2;
                    hashx<string, int> hastop;
                    while (getann(0, &z) == 0) {
                        bool b_store = z.time >= t_start && z.time < t_end;
                        sleep_info si;
                        hastop.hashx_clear();

                        string s = string((char *) z.aux + 1, size_t((unsigned char) *z.aux));
                        vector<string> anns = splitToVector(s, " ");

                        for (size_t i = 0; i < anns.size(); ++i) {
                            s.clear();
                            for (size_t k = 0; k < anns[i].length(); ++k) {
                                char c = anns[i][k];
                                if (int(c) >= 32 && c != ' ') { s += c; }
                            }
                            if (s.empty()) { continue; }
                            if (keys.find(s)) {
                                si.istage = keys[s];
                                if (si.istage == -3) { si.mt = true; }
                                if (si.istage <= -2) { si.istage = ph_prev; }
                                si.nw = 30 * fd;
                            } else {
                                if (hastop.insert(s) == 1) { si.addann.push_back(s); }
                            }
                        }
                        ph_prev = si.istage;

                        if (b_store && !si.empty()) {
                            dat.slpst[z.time - t_start] = si;
                            si.clear();
                        }
                    }
                }

                if (b_save_beats) {
                    int res2;
                    string annot_class = "ecg";
                    enum {
                        nanns_max = 1
                    };
                    WFDB_Anninfo anns[nanns_max];
                    anns[0].name = &annot_class[0];
                    anns[0].stat = WFDB_READ;
                    res2 = annopen(&name_src_wfdb[0], &anns[0], nanns_max);
                    if (res2 != 0) {
                        res = 19;
                        break;
                    }

                    WFDB_Annotation z;
                    while (getann(0, &z) == 0) {
                        bool b_store = z.time >= t_start && z.time < t_end;
                        if (b_store && z.anntyp == NORMAL) { dat.beats.insert(z.time - t_start); }
                    }
                }

                res = dat.save(fnp_dest, sec, b_append);
                if (!res) {
                    res = 18;
                    break;
                }
                res = 0;
                break; // on_calm_result
            }

        if (res) {
            cerr << "ERR [wfdb]: " << res << endl;
            return res;
        }

        if (!fnp_src_tdb.isEmpty())
            while (1) // once
            {
                ecg_data dat2(250);

                fnp_src_tdb.rx<utStringArray>();
                fnp_src_tdb.arrlb_(0);
                res = 0;
                for (s_long i = 0; i < fnp_src_tdb.arrsz(); ++i) {
                    res = dat2.load(fnp_src_tdb.vcstr(i), sec);
                    if (!res) {
                        res = i * 100 + 20;
                        break;
                    }
                    if (i == 0 && (t_start > 0 || t_end >= 0)) { dat2.trim(t_start, t_end); }
                    res = dat2.save(fnp_dest, sec, b_append);
                    if (!res) {
                        res = i * 100 + 21;
                        break;
                    }
                    res = 0;
                }
                break;
            }
        if (res) {
            cerr << "ERR [text db]: " << res << endl;
            return res;
        }

        return 0;
    }
    catch (const std::exception &e) { cerr << "ERR main(): C++ exception: " << e.what() << endl; }
    catch (...) { cerr << "ERR main(): catch(...)" << endl; }
    return 1;
}
