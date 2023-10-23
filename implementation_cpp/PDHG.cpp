#include "shared_functions.h"

RecordIterates *PDHG(const Params &p)
{
	int size_x = (int)p.c.rows();
	int size_y = (int)p.b.rows();
	Iterates iter(size_x, size_y);
	auto record = new RecordIterates(size_x, size_y, p.max_iter / p.record_every);
	while (true)
	{
		PDHGStep(iter, p, *record);
		// AdaptiveRestarts(iter, p, *record);
		// FixedFrequencyRestart(iter, p, *record, 1024);
		if (iter.terminate || iter.count > p.max_iter)
			break;
	}

	record->saveConvergeinfo(__func__, p.dataidx, "no_restarts");
	// record->saveRestart_idx(__func__, p.dataidx, "adaptive_restarts");
	return record;
}

void PDHGStep(Iterates &iter, const Params &p, RecordIterates &record)
{
	Timer timer;
	Eigen::VectorXd x_new = (iter.x - (p.eta / p.w) * (p.c - p.A.transpose() * iter.y)).cwiseMax(0);
	Eigen::VectorXd y_new = iter.y - p.eta * p.w * (-p.b + p.A * (2 * x_new - iter.x));
	timer.timing();

	iter.x = x_new;
	iter.y = y_new;

	iter.x_hat = x_new;
	iter.y_hat = y_new;
	iter.update();
	timer.timing();

	timer.save(__func__, p, iter.count);

	auto count = iter.count;
	if ((count - 1) % p.record_every == 0 || (count - 1) % p.print_every == 0)
	{
		iter.compute_convergence_information(p);
		if ((count - 1) % p.record_every == 0)
		{
			record.append(iter, p);
		}
		if ((count - 1) % p.print_every == 0 && p.verbose)
		{
			iter.print_iteration_information(p);
		}
	}
}
