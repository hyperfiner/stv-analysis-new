#pragma once

// STV analysis includes
#include "SystematicsCalculator.hh"

// Calculates covariance matrices describing the uncertainty on the reco-space
// event counts. Uses a "recipe" appropriate for unfolding via the Wiener-SVD
// or D'Agostini methods (e.g., cross-section systematic uncertainties are
// evaluated fully on backgrounds but enter only via the response matrix for
// signal events).
class MCC9Unfolder : public SystematicsCalculator {

  public:

    MCC9Unfolder( const std::string& input_respmat_file_name,
      const std::string& respmat_tdirectoryfile_name = "" );

    virtual double evaluate_observable( const Universe& univ, int reco_bin,
      int flux_universe_index = -1 ) const override;

    virtual double evaluate_mc_stat_unc( const Universe& univ,
      int reco_bin ) const override;

    virtual double evaluate_data_stat_unc( int reco_bin,
      bool use_ext ) const override;
};

MCC9Unfolder::MCC9Unfolder(
  const std::string& input_respmat_file_name,
  const std::string& respmat_tdirectoryfile_name )
  : SystematicsCalculator( input_respmat_file_name,
  respmat_tdirectoryfile_name )
{

}

double MCC9Unfolder::evaluate_observable( const Universe& univ, int reco_bin,
  int flux_universe_index ) const
{
  // For the MCC9Unfolder class, the observable of interest is the total number
  // of events (signal + background) in the current bin in reco space
  double reco_bin_events = 0.;

  // Get access to the CV universe. We need it regardless of the input universe
  // so that we can use it in the denominator of the smearceptance matrix
  // element
  const Universe& cv_univ = this->cv_universe();

  size_t num_true_bins = true_bins_.size();

  // We need to sum the contributions of the various true bins,
  // so loop over them while checking whether each one is associated
  // with either signal or background
  for ( size_t tb = 0u; tb < num_true_bins; ++tb ) {
    const auto& tbin = true_bins_.at( tb );

    if ( tbin.type_ == kSignalTrueBin ) {

      // Get the CV event count for the current true bin
      double denom_CV = cv_univ.hist_true_->GetBinContent( tb + 1 );

      // For the systematic variation universes, we want to assess
      // uncertainties on the signal only through the smearceptance
      // matrix. We therefore compute the smearceptance matrix element
      // here and then apply it to the CV expected event count in
      // each true bin.
      // NOTE: ROOT histogram bin numbers are one-based (bin zero is always
      // the underflow bin). Our bin indices therefore need to be offset by
      // +1 in all cases here.
      double numer = univ.hist_2d_->GetBinContent( tb + 1, reco_bin + 1 );
      double denom = univ.hist_true_->GetBinContent( tb + 1 );

      // I plan to extract the flux-averaged cross sections in terms of the
      // *nominal* flux model (as opposed to the real flux). I therefore
      // vary the numerator of the smearceptance matrix for these while
      // keeping the denominator equal to the CV expectation under the
      // nominal flux model. This is the same strategy as is used in the
      // Wire-Cell CC inclusive analysis.
      if ( flux_universe_index >= 0 ) {
        denom = denom_CV;
      }

      // If the denominator is nonzero actually calculate the fraction.
      // Otherwise, just leave it zeroed out.
      // TODO: revisit this, think about MC statistical uncertainties
      // on the empty bins
      double smearcept = 0.;
      if ( denom > 0. ) smearcept = numer / denom;

      // Compute the expected signal events in this universe
      // by multiplying the varied smearceptance matrix element
      // by the unaltered CV prediction in the current true bin.
      double expected_signal = smearcept * denom_CV;

      // Compute the expected signal events in the current reco bin
      // with the varied smearceptance matrix (and, for flux universes,
      // the varied integrated flux)
      reco_bin_events += expected_signal;
    }
    else if ( tbin.type_ == kBackgroundTrueBin ) {
      // For background events, we can use the same procedure regardless
      // of whether we're in the CV universe or not
      double background = univ.hist_2d_->GetBinContent( tb + 1, reco_bin + 1 );
      reco_bin_events += background;
    }
  } // true bins

  return reco_bin_events;
}

double MCC9Unfolder::evaluate_mc_stat_unc( const Universe& univ,
  int reco_bin ) const
{
  // ROOT histograms use one-based bin indices, so I correct for that here
  double err = univ.hist_reco_->GetBinError( reco_bin + 1 );
  return err;
}

double MCC9Unfolder::evaluate_data_stat_unc( int reco_bin, bool use_ext ) const
{
  const TH1D* d_hist = nullptr;
  if ( use_ext ) d_hist = data_hists_.at( NFT::kExtBNB ).get(); // EXT data
  else d_hist = data_hists_.at( NFT::kOnBNB ).get(); // BNB data
  // ROOT histograms use one-based bin indices, so I correct for that here
  double err = d_hist->GetBinError( reco_bin + 1 );
  return err;
}