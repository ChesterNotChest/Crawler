#include "sqlinterface.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QDebug>

SQLInterface::SQLInterface() {}
SQLInterface::~SQLInterface() { disconnect(); }

bool SQLInterface::openSqliteConnection(const QString &dbFilePath) {
	const QString connName = QStringLiteral("crawler_conn");
	QSqlDatabase db;
	if (QSqlDatabase::contains(connName)) {
		db = QSqlDatabase::database(connName);
	} else {
		db = QSqlDatabase::addDatabase("QSQLITE", connName);
	}
	db.setDatabaseName(dbFilePath);
	if (!db.open()) {
		qDebug() << "SQLite open failed:" << db.lastError().text();
		return false;
	}
	return true;
}

bool SQLInterface::connectSqlite(const QString &dbFilePath) {
	return openSqliteConnection(dbFilePath);
}

bool SQLInterface::isConnected() const {
	const QString connName = QStringLiteral("crawler_conn");
	if (!QSqlDatabase::contains(connName)) return false;
	const QSqlDatabase db = QSqlDatabase::database(connName);
	return db.isValid() && db.isOpen();
}

void SQLInterface::disconnect() {
	const QString connName = QStringLiteral("crawler_conn");
	if (!QSqlDatabase::contains(connName)) return;
	{
		QSqlDatabase db = QSqlDatabase::database(connName);
		if (db.isOpen()) db.close();
	}
	QSqlDatabase::removeDatabase(connName);
}

bool SQLInterface::createAllTables() {
	if (!isConnected()) {
		qDebug() << "Not connected; cannot create tables.";
		return false;
	}
	const QString connName = QStringLiteral("crawler_conn");
	QSqlDatabase db = QSqlDatabase::database(connName);

	// RecruitType (fixed enum: 1=校招, 2=实习, 3=社招)
	QSqlQuery q(db);
	if (!q.exec(
		"CREATE TABLE IF NOT EXISTS RecruitType ("
		" recruitTypeId INTEGER PRIMARY KEY,"
		" typeName TEXT NOT NULL UNIQUE"
		")")) {
		qDebug() << "Create RecruitType failed:" << q.lastError().text();
		return false;
	}

	// Company
	if (!q.exec(
		"CREATE TABLE IF NOT EXISTS Company ("
		" companyId INTEGER PRIMARY KEY,"
		" companyName TEXT NOT NULL"
		")")) {
		qDebug() << "Create Company failed:" << q.lastError().text();
		return false;
	}

	// JobCity
	if (!q.exec(
		"CREATE TABLE IF NOT EXISTS JobCity ("
		" cityId INTEGER PRIMARY KEY AUTOINCREMENT,"
		" cityName TEXT NOT NULL UNIQUE"
		")")) {
		qDebug() << "Create JobCity failed:" << q.lastError().text();
		return false;
	}

	// JobTag
	if (!q.exec(
		"CREATE TABLE IF NOT EXISTS JobTag ("
		" tagId INTEGER PRIMARY KEY AUTOINCREMENT,"
		" tagName TEXT NOT NULL UNIQUE"
		")")) {
		qDebug() << "Create JobTag failed:" << q.lastError().text();
		return false;
	}

	// SalarySlab (薪资档次)
	if (!q.exec(
		"CREATE TABLE IF NOT EXISTS SalarySlab ("
		" salarySlabId INTEGER PRIMARY KEY,"
		" maxSalary INTEGER NOT NULL"
		")")) {
		qDebug() << "Create SalarySlab failed:" << q.lastError().text();
		return false;
	}

	// Job (主表)
	if (!q.exec(
		"CREATE TABLE IF NOT EXISTS Job ("
		" jobId INTEGER PRIMARY KEY,"
		" jobName TEXT NOT NULL,"
		" companyId INTEGER,"
		" recruitTypeId INTEGER,"
		" cityId INTEGER,"
		" requirements TEXT,"
		" salaryMin REAL,"
		" salaryMax REAL,"
		" salarySlabId INTEGER,"
		" createTime TEXT,"
		" updateTime TEXT,"
		" hrLastLoginTime TEXT"
		")")) {
		qDebug() << "Create Job failed:" << q.lastError().text();
		return false;
	}

	// JobTagMapping (many-to-many)
	if (!q.exec(
		"CREATE TABLE IF NOT EXISTS JobTagMapping ("
		" jobId INTEGER,"
		" tagId INTEGER,"
		" PRIMARY KEY (jobId, tagId)"
		")")) {
		qDebug() << "Create JobTagMapping failed:" << q.lastError().text();
		return false;
	}

	// Initialize RecruitType enum values if empty
	if (!q.exec("SELECT COUNT(*) FROM RecruitType")) {
		qDebug() << "Count RecruitType failed:" << q.lastError().text();
		return false;
	}
	q.next();
	if (q.value(0).toInt() == 0) {
		q.prepare("INSERT INTO RecruitType(recruitTypeId, typeName) VALUES(:id, :name)");
		const QVector<QPair<int, QString>> types{{1, "校招"}, {2, "实习"}, {3, "社招"}};
		for (const auto &t : types) {
			q.bindValue(":id", t.first);
			q.bindValue(":name", t.second);
			if (!q.exec()) {
				qDebug() << "Insert RecruitType failed:" << q.lastError().text();
				return false;
			}
		}
	}

	qDebug() << "All tables created successfully.";
	return true;
}

int SQLInterface::insertCompany(int companyId, const QString &companyName) {
	if (!isConnected()) return -1;
	const QString connName = QStringLiteral("crawler_conn");
	QSqlDatabase db = QSqlDatabase::database(connName);
	QSqlQuery q(db);
	q.prepare("INSERT INTO Company(companyId, companyName) VALUES(:id, :name)");
	q.bindValue(":id", companyId);
	q.bindValue(":name", companyName);
	if (!q.exec()) {
		qDebug() << "Insert Company failed:" << q.lastError().text();
		return -1;
	}
	return companyId;
}

int SQLInterface::insertCity(const QString &cityName) {
	if (!isConnected()) return -1;
	const QString connName = QStringLiteral("crawler_conn");
	QSqlDatabase db = QSqlDatabase::database(connName);
	QSqlQuery q(db);
	q.prepare("INSERT OR IGNORE INTO JobCity(cityName) VALUES(:name)");
	q.bindValue(":name", cityName);
	q.exec(); // ignore duplicates
	q.prepare("SELECT cityId FROM JobCity WHERE cityName = :name");
	q.bindValue(":name", cityName);
	if (q.exec() && q.next()) {
		return q.value(0).toInt();
	}
	return -1;
}

int SQLInterface::insertTag(const QString &tagName) {
	if (!isConnected()) return -1;
	const QString connName = QStringLiteral("crawler_conn");
	QSqlDatabase db = QSqlDatabase::database(connName);
	QSqlQuery q(db);
	q.prepare("INSERT OR IGNORE INTO JobTag(tagName) VALUES(:name)");
	q.bindValue(":name", tagName);
	q.exec(); // ignore duplicates
	q.prepare("SELECT tagId FROM JobTag WHERE tagName = :name");
	q.bindValue(":name", tagName);
	if (q.exec() && q.next()) {
		return q.value(0).toInt();
	}
	return -1;
}

bool SQLInterface::insertSalarySlab(int slabId, int maxSalary) {
	if (!isConnected()) return false;
	const QString connName = QStringLiteral("crawler_conn");
	QSqlDatabase db = QSqlDatabase::database(connName);
	QSqlQuery q(db);
	q.prepare("INSERT OR IGNORE INTO SalarySlab(salarySlabId, maxSalary) VALUES(:id, :max)");
	q.bindValue(":id", slabId);
	q.bindValue(":max", maxSalary);
	return q.exec();
}

int SQLInterface::insertJob(const SQLNS::JobInfo &job) {
	if (!isConnected()) return -1;
	const QString connName = QStringLiteral("crawler_conn");
	QSqlDatabase db = QSqlDatabase::database(connName);
	QSqlQuery q(db);
	q.prepare(
		"INSERT INTO Job(jobId, jobName, companyId, recruitTypeId, cityId, "
		"requirements, salaryMin, salaryMax, salarySlabId, createTime, updateTime, hrLastLoginTime) "
		"VALUES(:jobId, :jobName, :companyId, :recruitTypeId, :cityId, "
		":requirements, :salaryMin, :salaryMax, :salarySlabId, :createTime, :updateTime, :hrLastLoginTime)");
	q.bindValue(":jobId", job.jobId);
	q.bindValue(":jobName", job.jobName);
	q.bindValue(":companyId", job.companyId);
	q.bindValue(":recruitTypeId", job.recruitTypeId);
	q.bindValue(":cityId", job.cityId);
	q.bindValue(":requirements", job.requirements);
	q.bindValue(":salaryMin", job.salaryMin);
	q.bindValue(":salaryMax", job.salaryMax);
	q.bindValue(":salarySlabId", job.salarySlabId);
	q.bindValue(":createTime", job.createTime);
	q.bindValue(":updateTime", job.updateTime);
	q.bindValue(":hrLastLoginTime", job.hrLastLoginTime);
	if (!q.exec()) {
		qDebug() << "Insert Job failed:" << q.lastError().text();
		return -1;
	}
	return job.jobId;
}

bool SQLInterface::insertJobTagMapping(int jobId, int tagId) {
	if (!isConnected()) return false;
	const QString connName = QStringLiteral("crawler_conn");
	QSqlDatabase db = QSqlDatabase::database(connName);
	QSqlQuery q(db);
	q.prepare("INSERT OR IGNORE INTO JobTagMapping(jobId, tagId) VALUES(:jobId, :tagId)");
	q.bindValue(":jobId", jobId);
	q.bindValue(":tagId", tagId);
	return q.exec();
}

QVector<SQLNS::JobInfo> SQLInterface::queryAllJobs() {
	QVector<SQLNS::JobInfo> jobs;
	if (!isConnected()) return jobs;
	const QString connName = QStringLiteral("crawler_conn");
	QSqlDatabase db = QSqlDatabase::database(connName);
	QSqlQuery q(db);
	if (!q.exec("SELECT jobId, jobName, companyId, recruitTypeId, cityId, requirements, "
				"salaryMin, salaryMax, salarySlabId, createTime, updateTime, hrLastLoginTime "
				"FROM Job ORDER BY jobId ASC")) {
		qDebug() << "Select Job failed:" << q.lastError().text();
		return jobs;
	}
	while (q.next()) {
		SQLNS::JobInfo job;
		job.jobId = q.value(0).toInt();
		job.jobName = q.value(1).toString();
		job.companyId = q.value(2).toInt();
		job.recruitTypeId = q.value(3).toInt();
		job.cityId = q.value(4).toInt();
		job.requirements = q.value(5).toString();
		job.salaryMin = q.value(6).toDouble();
		job.salaryMax = q.value(7).toDouble();
		job.salarySlabId = q.value(8).toInt();
		job.createTime = q.value(9).toString();
		job.updateTime = q.value(10).toString();
		job.hrLastLoginTime = q.value(11).toString();

		// Query tags for this job
		QSqlQuery tagQuery(db);
		tagQuery.prepare("SELECT t.tagId FROM JobTag t "
						"JOIN JobTagMapping m ON t.tagId = m.tagId "
						"WHERE m.jobId = :jobId");
		tagQuery.bindValue(":jobId", job.jobId);
		if (tagQuery.exec()) {
			while (tagQuery.next()) {
				job.tagIds.append(tagQuery.value(0).toInt());
			}
		}

		jobs.append(job);
	}
	return jobs;
}

SQLNS::SalaryRange SQLInterface::getSalarySlabRange(int slabId) {
	SQLNS::SalaryRange range{0, 0};
	if (!isConnected()) return range;
	const QString connName = QStringLiteral("crawler_conn");
	QSqlDatabase db = QSqlDatabase::database(connName);
	QSqlQuery q(db);
	q.prepare("SELECT maxSalary FROM SalarySlab WHERE salarySlabId = :id");
	q.bindValue(":id", slabId);
	if (q.exec() && q.next()) {
		range.maxSalary = q.value(0).toInt();
	}
	return range;
}
