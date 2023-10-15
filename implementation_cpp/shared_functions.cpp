#include "shared_functions.h"

Iterates::Iterates(const int& Size_x, const int& Size_y) : n(0), t(0), count(1),
terminate{ false }
{
	size_x = Size_x;
	size_y = Size_y;
	size_z = size_x + size_y;
	z = Eigen::VectorXd::Zero(size_z);
	z_hat = Eigen::VectorXd::Zero(size_z);
	z_bar = Eigen::VectorXd::Zero(size_z);
	this->use_ADMM = false;
	this->cache.z_cur_start = this->z;
	this->now_time();
}

Iterates::Iterates(const int& Repeat_x, const int& Size_x, const int& Size_y) :
	n(0), t(0), count(1), terminate{ false }
{
	size_x = Size_x;
	size_y = Size_y;
	size_z = Repeat_x * size_x + size_y;
	z = Eigen::VectorXd::Random(size_z);
	z_hat = Eigen::VectorXd::Random(size_z);
	z_bar = Eigen::VectorXd::Random(size_z);
	this->use_ADMM = true;
	this->cache.z_cur_start = this->z;
	this->now_time();
}

void Iterates::now_time()
{
	time = high_resolution_clock::now();
}

float Iterates::timing()
{
	auto now = high_resolution_clock::now();
	auto duration = duration_cast<milliseconds>(now - time).count();
	time = now;
	return duration / 1e3;
}

void Iterates::update()
{
	z_bar = t * 1.0 / (t + 1) * z_bar + 1.0 / (t + 1) * z_hat;
	t++;
	count++;
}

void Iterates::restart()
{
	n++;
	t = 0;
	count++;
	z = z_bar;

	cache.z_prev_start = cache.z_cur_start;
	cache.z_cur_start = this->z;
}

Convergeinfo Iterates::compute_convergence_information(const Params& p)
{
	Eigen::VectorXd y = gety();
	Eigen::VectorXd c = p.c; Eigen::VectorXd b = p.b; Eigen::SparseMatrix<double> A = p.A;
	if (use_ADMM == false) {
		Eigen::VectorXd x = getx();
		Eigen::VectorXd kkt_error_vec(2 * size_x + 2 * size_y + 1);
		kkt_error_vec << -x, A* x - b, b - A * x, A.transpose()* y - c,
			c.dot(x) - b.dot(y);
		this->convergeinfo.kkt_error = (kkt_error_vec.cwiseMax(0)).norm();
	}
	else {
		Eigen::VectorXd xU = getxU();
		Eigen::VectorXd xV = getxV();
		Eigen::VectorXd kkt_error_vec(3 * size_x + 2 * p.b.rows());
		kkt_error_vec << -xV, A* xU - b, b - A * xU, xU - xV, xV - xU;
		this->convergeinfo.kkt_error = (kkt_error_vec.cwiseMax(0)).norm();
	}

	double r = (this->z - this->cache.z_cur_start).norm();
	//std::cout << this->count - 1 << " r = " << r << std::endl;
	//this->convergeinfo.normalized_duality_gap = compute_normalized_duality_gap(this->z_bar, r + 1e-6, p);
	if (std::abs(this->convergeinfo.normalized_duality_gap) < p.tol)
	{
		this->terminate = true;
	}
	return this->convergeinfo;
}

void Iterates::print_iteration_information(const Params& p)
{
	std::cout << "Iteration " << count - 1 << ", ";
	std::cout << "kkt_error: " << this->convergeinfo.kkt_error << std::endl;
	std::cout << "normalized_duality_gap: " << this->convergeinfo.normalized_duality_gap << std::endl;

	if (use_ADMM == false) {
		Eigen::VectorXd x = getx();
		Eigen::VectorXd y = gety();
		std::cout << "obj: " << p.c.dot(x) + p.b.dot(y)
			- y.transpose() * p.A * x << std::endl;
	}
	else {
		Eigen::VectorXd xU = getxU();
		Eigen::VectorXd xV = getxV();
		Eigen::VectorXd y = gety();
		std::cout << xU.norm() << " " << xV.norm() << " " << (xU - xV).norm() << " " << y.norm() << std::endl;
		std::cout << "obj: " << p.c.dot(xV) - y.dot(xU - xV) << std::endl;
	}

	std::cout << "Iterations take " << this->timing() << "s" << std::endl;
	std::cout << std::endl;
}

Eigen::VectorXd Iterates::getx() const
{
	return z.head(size_x);
}

Eigen::VectorXd Iterates::gety() const
{
	return z.tail(size_y);
}

Eigen::VectorXd Iterates::getxU() const
{
	return z.head(size_x);
}

Eigen::VectorXd Iterates::getxV() const
{
	return z(Eigen::seq(size_x, 2 * size_x - 1));
}

RecordIterates::RecordIterates(const int& Size_x, const int& Size_y, const int& Size_record)
	: end_idx(0)
{
	Iterates iter(Size_x, Size_y);
	std::vector<Iterates> aIteratesList(Size_record, iter);
	std::vector<Convergeinfo> aConvergeinfoList(Size_record);
	IteratesList = aIteratesList;
	ConvergeinfoList = aConvergeinfoList;
	this->use_ADMM = false;
}

RecordIterates::RecordIterates(const int& Repeat_x, const int& Size_x, const int& Size_y, const int& Size_record)
	: end_idx(0)
{
	Iterates iter(Repeat_x, Size_x, Size_y);
	std::vector<Iterates> aIteratesList(Size_record, iter);
	std::vector<Convergeinfo> aConvergeinfoList(Size_record);
	IteratesList = aIteratesList;
	ConvergeinfoList = aConvergeinfoList;
	this->use_ADMM = true;
}

void RecordIterates::append(const Iterates& iter, const Params& p)
{
	IteratesList[end_idx] = iter;
	ConvergeinfoList[end_idx] = iter.convergeinfo;
	end_idx++;
}

Iterates RecordIterates::operator[](const int& i) {
	if (i < 0 || i >= end_idx) {
		throw std::out_of_range("Index out of range");
	}
	return IteratesList[i];
}

void RecordIterates::saveConvergeinfo(const std::string method, const int dataidx, const std::string filename)
{
	auto path = cachepath + method + "/" + Data[dataidx] + "/";
	std::filesystem::create_directories(path);
	std::ofstream ofs(path + filename + cachesuffix);
	if (ofs.is_open())
	{
		/*boost::archive::xml_oarchive xml_output_archive(ofs);
		xml_output_archive& BOOST_SERIALIZATION_NVP(this->ConvergeinfoList);*/
		for (int i = 0; i < end_idx; i++)
		{
			auto& obj = ConvergeinfoList[i];
			ofs << std::setprecision(10) << obj.normalized_duality_gap << "," << obj.kkt_error << std::endl;
		}
		std::cout << "save Convergeinfo done" << std::endl;
		ofs.close();
	}
	else std::cout << "Unable to open file" << std::endl;
}

void RecordIterates::saveRestart_idx(const std::string method, const int dataidx, const std::string filename)
{
	auto path = cachepath + method + "/" + Data[dataidx] + "/";
	std::filesystem::create_directories(path);
	std::ofstream ofs(path + filename + "_restart_idx" + cachesuffix);
	if (ofs.is_open())
	{
		for (auto i : restart_idx)
		{
			ofs << i << std::endl;
		}
		std::cout << "save restart_idx done" << std::endl;
		ofs.close();
	}
	else std::cout << "Unable to open file" << std::endl;
}

Params::Params() : env(GRBEnv()), eta(1e-1), beta(std::exp(-1)), w(1), tol(1e-7),
max_iter(static_cast<int>(5e5)), tau0(1), verbose(false), restart(true),
record_every(30), print_every(100), evaluate_every(30), dataidx(0)
{
	env.set(GRB_IntParam_OutputFlag, verbose);
}

void Params::set_verbose(const bool& Verbose, const bool& gbVerbose)
{
	verbose = Verbose;
	env.set(GRB_IntParam_OutputFlag, gbVerbose);
}

void Params::load_example()
{
	Eigen::VectorXd c_tmp(5);
	c_tmp << -4, -3, 0, 0, 0;
	c = c_tmp;
	Eigen::VectorXd b_tmp(2);
	b_tmp << 4, 5;
	b = b_tmp;
	Eigen::SparseMatrix<double> A_tmp(2, 5);
	A_tmp.insert(0, 0) = 1;
	A_tmp.insert(0, 2) = 1;
	A_tmp.insert(1, 0) = 1;
	A_tmp.insert(1, 1) = 1;
	A = A_tmp;
	// solution is (4, 1, 0, 0, 0)
}

void Params::load_model(const int& dataidx)
{
	this->dataidx = dataidx;
	GRBModel model = GRBModel(env, datapath + Data[dataidx] + datasuffix);
	//model.optimize();

	// Get the number of variables in the model.
	int numVars = model.get(GRB_IntAttr_NumVars);

	// Get the number of constraints in the model.
	int numConstraints = model.get(GRB_IntAttr_NumConstrs);

	GRBVar* Vars = model.getVars();
	GRBConstr* Constrs = model.getConstrs();

	// Get the object coefficients from the model.
	c = Eigen::Map<Eigen::VectorXd>(model.get(GRB_DoubleAttr_Obj, Vars, numVars), numVars);

	// Get the matrix A, use sparse representation.
	Eigen::SparseMatrix<double, Eigen::RowMajor> A_tmp(numConstraints, numVars);
	std::vector<Eigen::Triplet<double>> triplets;

	for (int i = 0; i < numConstraints; i++)
	{
		for (int j = 0; j < numVars; j++)
		{
			double tmp = model.getCoeff(Constrs[i], Vars[j]);
			if (tmp != 0.0)
			{
				triplets.push_back(Eigen::Triplet<double>(i, j, tmp));
			}
		}
	}

	A_tmp.setFromTriplets(triplets.begin(), triplets.end());
	A = A_tmp;

	// Get the right-hand side vector from the model.
	b = Eigen::Map<Eigen::VectorXd>(model.get(GRB_DoubleAttr_RHS,
		model.getConstrs(), numConstraints), numConstraints);

	std::cout << "Model loaded." << std::endl;
}

Eigen::VectorXd& z_hat_lambda(const double& lambda, const Eigen::VectorXd& z,
	const Eigen::VectorXd& g, const Eigen::VectorXd& l)
{
	static Eigen::VectorXd z_hat = (z - lambda * g).cwiseMax(l);
	return z_hat;
}

Eigen::VectorXd& LinearObjectiveTruseRegion(const Eigen::VectorXd& g, const Eigen::VectorXd& l,
	const Eigen::VectorXd& z, const double& r)
{
	static Eigen::VectorXd z_hat;
	if ((l - z).norm() <= r) {
		z_hat = l;
		return z_hat;
	}

	double lambda_mid{ -1 }, f_low{ 0 }, f_high{ 0 }, f_mid{ -1 };
	Eigen::VectorXd z_hat, idx;

	Eigen::VectorXd lambdalist = (z - l).cwiseQuotient(g);
	Eigen::VectorXd lambdalist_sorted = (z - l).cwiseQuotient(g);
	std::sort(lambdalist_sorted.data(), lambdalist_sorted.data() + lambdalist_sorted.size());

	auto size_z = (int)z.size();
	int low{ 0 }, high{ size_z - 1 }, mid{ -1 };
	auto r_squared = std::pow(r, 2.0);

	while (low < high) {
		mid = std::ceil((low + high) / 2.0 - 1);
		lambda_mid = lambdalist_sorted[mid];
		f_mid = 0;
		z_hat = z_hat_lambda(lambda_mid, z, g, l);
		f_mid = f_low + f_high * std::pow(lambda_mid, 2.0) + (z_hat - z).segment(low, high).squaredNorm();
		if (f_mid <= r_squared) {
			low = mid + 1;
			idx = (lambdalist.array() <= lambda_mid).cast<double>();
			f_low += (l - z).cwiseProduct(idx).squaredNorm();
		}
		else {
			high = mid - 1;
			idx = (lambdalist.array() >= lambda_mid).cast<double>();
			f_high += g.cwiseProduct(idx).squaredNorm();
		}
	}
	lambda_mid = std::sqrt((r_squared - f_low) / f_high);
	z_hat = z_hat_lambda(lambda_mid, z, g, l);

	return z_hat;
}

double compute_normalized_duality_gap(const Eigen::VectorXd& z0, const double& r, const Params& p)
{
	int size_x = (int)p.c.rows();
	int size_y = (int)p.b.rows();

	Eigen::VectorXd x0 = z0.head(size_x);
	Eigen::VectorXd y0 = z0.tail(size_y);

	Eigen::VectorXd y_coeff = p.b - p.A * x0;
	Eigen::VectorXd x_coeff = y0.transpose() * p.A - p.c.transpose();

	double constant = p.c.dot(x0) - p.b.dot(y0);

	//std::cout << y_coeff << x_coeff << constant << std::endl;

	GRBModel model = GRBModel(p.env);

	// Create variables
	GRBVar* x = model.addVars(size_x, GRB_CONTINUOUS);
	GRBVar* y = new GRBVar[size_y];
	for (int i = 0; i < size_y; i++)
	{
		y[i] = model.addVar(-GRB_INFINITY, GRB_INFINITY, 0.0, GRB_CONTINUOUS);
	}

	// Create objective
	GRBLinExpr objExpr = GRBLinExpr();
	objExpr.addTerms(y_coeff.data(), y, size_y);
	objExpr.addTerms(x_coeff.data(), x, size_x);
	objExpr += constant;

	// Set objective
	model.setObjective(objExpr, GRB_MAXIMIZE);

	// Create constraints
	GRBQuadExpr ConstrExpr = GRBQuadExpr();
	Eigen::VectorXd x_quaCoeff = Eigen::VectorXd::Ones(size_x);
	Eigen::VectorXd y_quaCoeff = Eigen::VectorXd::Ones(size_y);
	Eigen::VectorXd x_linCoeff = -2 * x0;
	Eigen::VectorXd y_linCoeff = -2 * y0;
	ConstrExpr.addTerms(x_quaCoeff.data(), x, x, size_x);
	ConstrExpr.addTerms(y_quaCoeff.data(), y, y, size_y);
	ConstrExpr.addTerms(x_linCoeff.data(), x, size_x);
	ConstrExpr.addTerms(y_linCoeff.data(), y, size_y);
	ConstrExpr += x0.squaredNorm() + y0.squaredNorm();

	// Add constraints
	model.addQConstr(ConstrExpr, GRB_LESS_EQUAL, r * r);

	model.optimize();

	/*std::cout << model.get(GRB_DoubleAttr_ObjVal) << std::endl;
	std::cout << x[0].get(GRB_DoubleAttr_X) << std::endl;
	std::cout << y[0].get(GRB_DoubleAttr_X) << std::endl;*/

	return model.get(GRB_DoubleAttr_ObjVal) / r;
}

void AdaptiveRestarts(Iterates& iter, const Params& p,
	RecordIterates& record)
{
	if ((iter.count - 1) % p.evaluate_every != 0 || p.restart == false) return;

	bool restart = false;
	if (iter.n == 0)
	{
		if (iter.t >= p.tau0)
		{
			restart = true;
		}
	}
	else
	{
		// ||z_bar^n,t-z^n,0||
		double r1 = (iter.z_bar - iter.cache.z_cur_start).norm();
		// ||z^n,0-z^n-1,0||
		double r2 = (iter.cache.z_cur_start - iter.cache.z_prev_start).norm();
		double duality_gap1 = compute_normalized_duality_gap(iter.z_bar, r1, p);
		double duality_gap2 = compute_normalized_duality_gap(iter.cache.z_cur_start, r2, p);
		if (duality_gap1 <= p.beta * duality_gap2)
		{
			restart = true;
		}
	}

	if (restart == true)
	{
		record.restart_idx.push_back(iter.count - 1);
		iter.print_iteration_information(p);
		//if ((iter.count - 1) % p.record_every == 0) record.append(iter, p);

		iter.compute_convergence_information(p);
		iter.restart();
	}

}

void FixedFrequencyRestart(Iterates& iter, const Params& p,
	RecordIterates& record, const int restart_length)
{
	if (p.restart == false) return;
	if ((iter.count - 1) % restart_length == 0)
	{
		iter.compute_convergence_information(p);
		iter.print_iteration_information(p);
		iter.restart();
		if ((iter.count - 1) % p.record_every == 0) record.append(iter, p);
	}

}

void GetBestFixedRestartLength(Params& p, RecordIterates(*method)(const Params&))
{
	auto best_gap = INFINITY;
	int best_length = 0;
	p.restart = false;
	std::vector<double> w_candidates;
	for (int i = 1; i < 10; i++)
	{
		w_candidates.push_back(std::pow(4, i));
	}
	for (int i = 0; i < w_candidates.size(); i++)
	{
	}
	return;
}

double PowerIteration(const Eigen::SparseMatrix<double>& A, const bool& verbose = false)
{
	int size = (int)A.rows();
	Eigen::VectorXd u = Eigen::VectorXd::Random(size);
	Eigen::VectorXd y = Eigen::VectorXd::Zero(size);
	double tol = 1e-12;
	int max_iter = 1000;
	int iter = 0;
	double lambda = 0;
	double lambda_prev = 0;

	while (true)
	{

		y = u / u.norm();
		u = A * y;
		lambda_prev = lambda;
		lambda = y.transpose() * u;

		iter++;
		if (std::abs(lambda - lambda_prev) / std::abs(lambda) < tol) {
			if (verbose) std::cout << "Power Iteration Converged in " << iter << " iterations." << std::endl;
			break;
		}
		else if (iter >= max_iter) {
			if (verbose) std::cout << "Maximum Iterations Reached." << std::endl;
			break;
		}
	}
	return lambda;
}

Eigen::VectorXd compute_F(const Eigen::VectorXd& z, const Params& p)
{
	Eigen::VectorXd x = z.head(p.c.rows());
	Eigen::VectorXd y = z.tail(p.b.rows());
	Eigen::VectorXd F(p.c.rows() + p.b.rows());
	Eigen::VectorXd nabla_xL = p.c - p.A.transpose() * y;
	Eigen::VectorXd nabla_yL = p.A * x - p.b;

	F << nabla_xL, nabla_yL;
	return F;
}

double GetOptimalw(Params& p, RecordIterates(*method)(const Params&))
{
	auto best_kkt_error = INFINITY;
	double best_w = 0;
	p.restart = false;
	std::vector<double> w_candidates;
	for (int i = -5; i < 6; i++)
	{
		w_candidates.push_back(std::pow(4, i));
	}
	for (int i = 0; i < w_candidates.size(); i++)
	{
		p.w = w_candidates[i];
		std::cout << "testing w=4^" << i - 5 << std::endl;
		RecordIterates record = method(p);
		double kkt_error = record.ConvergeinfoList[record.end_idx - 1].kkt_error;
		if (kkt_error < best_kkt_error)
		{
			best_kkt_error = kkt_error;
			best_w = p.w;
		}
	}
	if (p.verbose) std::cout << "the optimal w is: " << best_w << std::endl;
	return best_w;
}