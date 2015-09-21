#include <QtNetwork>
#include <QNetworkAccessManager>
#include <QUrl>
#include <QCoreApplication>
#include "rapidjson\document.h"
#include "rapidjson\writer.h"
#include "rapidjson\stringbuffer.h"
#include <cstdio>
#include <vector>

#define nearby_threshold (0.012)

#define API_KEY1 "76729635abec163182e329af82d41ca3"
#define API_KEY2 "190fc63204ac903f3dea40b327d87f49"
#define API_KEY3 "6715fcdf415fd9df0daeb79eea75084f"
#define API_KEY _api_key()
QString _api_key(){
    static int keyturn = 0;
    static const QString klist[] = { API_KEY1, API_KEY2, API_KEY3, };
    keyturn = (keyturn + 1) % 3;
    return klist[keyturn];
}

class getjson {

private:
    QUrl url;
    QNetworkAccessManager qnam;
    QNetworkReply* reply;
public:
    void set_url( QString& name, QString& city ) {
        QString surl = QString("http://restapi.amap.com/v3/place/text?key=") + API_KEY +
                       "&keywords=" + name +
                       "&city=" + city +
                       "&output=json&types=141201&offset=50";
        url.setUrl( surl, QUrl::ParsingMode::TolerantMode );
    }
    rapidjson::Document get();
};

rapidjson::Document getjson::get() {
    reply = qnam.get( QNetworkRequest( url ) );

    QEventLoop loop;
    QCoreApplication::connect( reply, SIGNAL( finished() ), &loop, SLOT( quit() ) );
    loop.exec();

    QString doc( reply->readAll() );
    rapidjson::Document j;

    if ( reply->error() ) {
        printf( "Import Fail" "Cannot connect AMAP API:\n%s\n", url.toString().toStdString().c_str() );
        return j;
    }

    j.Parse( doc.toStdString().c_str() );

    return j;
}



struct university_t{
    QString name;
    QString city;
    std::vector<QString> pname;
    std::vector<double> longitude;
    std::vector<double> latitude;
};

std::vector<university_t> ulist;

void lineparse(QString line){
    QStringList col = line.split(",");
    bool ok;
    col.value(0).toInt(&ok);
    if(!ok) return;
    university_t newu;
    newu.name = col.value(1);
    newu.city = col.value(3);
    ulist.push_back(newu);
}

void parse(FILE* f){
    char ch;
    char buf[1024];
    int i = 0;
    while((ch = fgetc(f)) != EOF){
        if(ch == '\r') continue;
        if(ch == '\n'){
            lineparse(QString::fromLocal8Bit(buf));
            i = 0;
            continue;
        }
        buf[i++] = ch;
        buf[i] = 0;
    }
    if(i) lineparse(QString::fromLocal8Bit(buf));
}

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    // Read csv file and parse universities.
    FILE* csv = fopen("unilist.csv", "rb");
    parse(csv);

    getjson gj;
    for(auto i = ulist.begin(); i!= ulist.end(); i++){
        gj.set_url(i->name, i->city);

        printf("%s, %s\n", i->name.toLocal8Bit().toStdString().c_str(), i->city.toLocal8Bit().toStdString().c_str());

        rapidjson::Document js = gj.get();
        if(!js["status"].GetInt()) continue;

        rapidjson::Value& poi = js["pois"];
        for(size_t j = 0; j < poi.Size(); j++){
            rapidjson::Value& p = poi[j];
            QString point_name = /*QString::fromLocal8Bit*/( p["name"].GetString() );
            if(!point_name.contains(i->name, Qt::CaseInsensitive)) continue;

            int ignore = 0;
            for(auto tocomp = ulist.begin(); tocomp != ulist.end(); tocomp++){
                if(tocomp != i && point_name.contains(tocomp->name, Qt::CaseInsensitive) && !i->name.contains(tocomp->name, Qt::CaseInsensitive)){
                    ignore = 1;
                    break;
                }
            }
            if(ignore) continue;
            ignore = 0;
            QString point_coord = /*QString::fromLocal8Bit*/( p["location"].GetString() );
            QStringList split_coord = point_coord.split(",");
            double lo = split_coord.value(0).toDouble();
            double la = split_coord.value(1).toDouble();
            for(size_t k = 0; k < i->pname.size(); k++){
                double dist = (i->longitude[k] - lo) * (i->longitude[k] - lo) + (i->latitude[k] - la) * (i->latitude[k] - la);
                dist = sqrt(dist);
                if (dist < nearby_threshold){
                    ignore = 1;
                    break;
                }
            }
            if(ignore) continue;
            i->pname.push_back(point_name);
            i->longitude.push_back(lo);
            i->latitude.push_back(la);
            printf("\t%s:(%f,%f)\n", point_name.toLocal8Bit().toStdString().c_str(), lo, la);
        }
    }

    rapidjson::StringBuffer s;
    rapidjson::Writer<rapidjson::StringBuffer> writer(s);

    writer.StartObject();
    writer.String("count");
    writer.Uint(ulist.size());
    writer.String("list");
    writer.StartArray();
    for(auto i = ulist.begin(); i!= ulist.end(); i++){
        writer.StartObject();
        writer.String("university");
        writer.String(i->name.toLocal8Bit().toStdString().c_str());
        writer.String("city");
        writer.String(i->city.toLocal8Bit().toStdString().c_str());
        writer.String("points");
        writer.StartArray();
        for(size_t j = 0; j < i->pname.size(); j++){
            writer.StartObject();
            writer.String("name");
            writer.String(i->pname[j].toLocal8Bit().toStdString().c_str());
            writer.String("longitude");
            writer.Double(i->longitude[j]);
            writer.String("latitude");
            writer.Double(i->latitude[j]);
            writer.EndObject();
        }
        writer.EndArray();
        writer.EndObject();
    }
    writer.EndArray();
    writer.EndObject();

    FILE* json = fopen("unilist.json", "wb");
    fprintf(json, s.GetString());
    fclose(json);

    return 0;
}

