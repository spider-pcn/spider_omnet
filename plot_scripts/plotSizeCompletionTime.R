#!/usr/bin/env Rscript
# This script can be run using:
# ./plot_multi_app.R <data> <synthetic_data>

library(ggplot2)
library(cowplot, warn.conflicts = FALSE)

args <- commandArgs(trailingOnly=TRUE)
file <- 'lndnewtopo_lnd_credit40_comp_time_data'
plot_prefix <- 'lndnewtopo_lnd_credit40_comp_time'
ytype <- "avg"
limit <- 4000
data<-read.csv(file)
range_data<-read.csv("size_range_data")
range_data$level <- as.factor(range_data$level)

label_list <- c("PS" = "Primal Dual Appraoch",
                 "SP" = "Shortest Path",
                 "LR" = "Landmark Routing",
                 "WF" = "Waterfilling", 
                 "LND" = "LND",
                 "DCTCP_qdelay_widest" = "DCTCP (Queue Delay widest)",
                 "DCTCP_qdelay_yen" = "DCTCP (Queue Delay yen)",
                 "DCTCP_qdelay" = "Spider")

break_list <- c("PS", "SP", "LR", "WF", "LND", "DCTCP_qdelay_widest", "DCTCP_qdelay", "DCTCP_qdelay_yen")

color_list <- c(
                "SP" = "#e51a1c",
                "PS" = "#377eb8",
                "LR" = "#4daf4a",
                "WF" ="#984ea3",
                "LND" = "#fb9a99",
                "DCTCP_qdelay_widest" = "#a65628",
                "DCTCP_qdelay" = "#253494",
                "DCTCP_qdelay_yen" = "#ff7f00")

range_list <- c(
                "1" = "5",
                "2" = "6 - 10",
                "3" = "11 - 15",
                "4" = "16 - 25",
                "5" = "26 - 41",
                "6" = "42 - 82",
                "7" = "83 - 164",
                "8" = "164 - 3930")

shape_list <- c(
                "PS" = 18,
                "LND" = 8,
                "LR" = 7,
                "SP" = 4,
                "WF" = 15,
                "DCTCP" = 17,
                "DCTCP_qdelay" = 19,
                "DCTCP_qdelay_widest" = 34,
                "DCTCP_qdelay_yen" = 36)

# identify which data to plot average 90% tail or 99%
if (ytype =="avg") {
    yaxis <- "AvgCompTime"
    ytitle <- "Average Completion Time (ms)"
    plot_filename <- paste(plot_prefix, "_avg.pdf", sep="")
} else if (ytype == "tail90") {
    yaxis <- "TailCompTime90"
    ytitle <- "90% Tail Completion Time (ms)"
    plot_filename <- paste(plot_prefix, "_tail90.pdf", sep="")
} else {
    yaxis <- "TailCompTime99"
    ytitle <- "99% Tail Completion Time (ms)"
    plot_filename <- paste(plot_prefix, "_tail99.pdf", sep="")
}


# add a line plot
avg_plot <- ggplot() + 
        geom_rect(data=range_data, 
                  aes_string(xmin="start", xmax="end", ymin=-Inf, ymax=Inf, fill="level"), 
                  alpha=0.5,
                  show.legend=FALSE) +
        geom_line(data=data, aes_string(x="Point",y="AvgCompTime", colour="Scheme"), size=1) +
        geom_point(data=data, aes_string(x="Point",y="AvgCompTime", colour="Scheme", shape="Scheme"), size=3) +
        
        scale_x_continuous(expand=c(0,0), trans="log2", breaks=c(5,10,15,25,41,82,164,3930)) +
        coord_cartesian(xlim=c(2,5000)) +
        
        labs(x="Transaction Size (Euros)", y="Average Latency (ms)") +
        scale_colour_manual(
                values=color_list,
                labels=label_list,
                breaks=break_list,
                guide=guide_legend(title=NULL, nrow=1)) +
        scale_shape_manual(
                    labels=label_list,
                    breaks=break_list,
                    values=shape_list,
                    guide=guide_legend(title=NULL, nrow =1)) +
        scale_fill_brewer(type = "seq", palette = "Blues", direction = 1,
                            aesthetics = "fill") +
  theme_minimal(base_size=22) + 
  theme(legend.text=element_text(size=rel(1.2)), 
        legend.key.size=unit(15,"points"), 
        legend.box.margin=margin(-10,-10,-10,-10), legend.margin=margin(c(5,5,1,5)),
        legend.position="top") +
  theme(axis.text.x = element_text(angle = 70, hjust = 1, size=15)) +
ggsave(paste(plot_prefix,"_avg.pdf", sep=""), width=5.7, height=5)

# add a line plot
tail_plot <- ggplot() + 
        geom_rect(data=range_data, 
                  aes_string(xmin="start", xmax="end", ymin=-Inf, ymax=Inf, fill="level"), 
                  alpha=0.5,
                  show.legend=FALSE) +
        geom_line(data=data, aes_string(x="Point",y="TailCompTime99", colour="Scheme"), size=1) +
        geom_point(data=data, aes_string(x="Point",y="TailCompTime99", colour="Scheme", shape="Scheme"), size=3) +
        
        scale_x_continuous(expand=c(0,0), trans="log2", breaks=c(5,10,15,25,41,82,164,3930)) +
        coord_cartesian(xlim=c(2,5000)) +
        
        labs(x="Transaction Size (Euros)", y="99%ile Latency (ms)") +
        scale_colour_manual(
                values=color_list,
                labels=label_list,
                breaks=break_list,
                guide=guide_legend(title=NULL, nrow=1)) +
        scale_shape_manual(
                    labels=label_list,
                    breaks=break_list,
                    values=shape_list,
                    guide=guide_legend(title=NULL, nrow =1)) +
        scale_fill_brewer(type = "seq", palette = "Blues", direction = 1,
                            aesthetics = "fill") +
  theme_minimal(base_size=22) + 
  theme(legend.text=element_text(size=rel(1.2)), 
        legend.key.size=unit(15,"points"), 
        legend.box.margin=margin(-10,-10,-10,-10), legend.margin=margin(c(5,5,1,5)),
        legend.position="top") +
  theme(axis.text.x = element_text(angle = 70, hjust = 1, size=15)) +
ggsave(paste(plot_prefix,"_tail99.pdf", sep=""), width=5.7, height=5)
legend <- get_legend(tail_plot + theme(legend.position="top"))


# put them together
prow <- plot_grid(avg_plot + theme(legend.position="none"),
                  tail_plot + theme(legend.position="none"), 
                  ncol = 2, align = "v", axis = "l")

# this tells it what order to put it in
# so basically tells it put legend first then plots with th legend height 20% of the 
# plot
p <- plot_grid(legend, prow, rel_heights=c(.2,1), ncol =1)
ggsave(paste(plot_prefix,"Summary.pdf", sep=""), width=12.2, height=5)

